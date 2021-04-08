/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */
/* Written by Dumitrel Loghin (2020) based on DynamoRIO samples.
 *
 * Gets the following floating-point values by inspecting source and
 * destination registers/memory locations:
 *
 * minimum value in (-inf,1]
 * maximum value in [-1,0)
 * minimum value in (0,1]
 * maximum value in [1,+inf)
 */

#include <string.h>
#include "dr_api.h"
#include "drmgr.h"
#ifdef SHOW_SYMBOLS
#    include "drsyms.h"
#endif
#include "utils.h"

#ifdef WINDOWS
#    define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
#    define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

#define NULL_TERMINATE(buf) (buf)[(sizeof((buf)) / sizeof((buf)[0])) - 1] = '\0'

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
		bool for_trace, bool translating, void *user_data);
static void
exit_event(void);
static void
event_thread_init(void *drcontext);
static void
event_thread_exit(void *drcontext);

static int tls_idx;
static client_id_t my_id;
#define DIM_MODULE_NAME	32
static char main_module_name[DIM_MODULE_NAME];

static float min_sup = -1.0;	// minimum value in (-inf,1]
static float max_sub = -1.0;  	// maximum value in [-1,0)
static float min_sub = 1.0;  	// minimum value in (0,1]
static float max_sup = 1.0;  	// maximum value in [1,+inf)

static void *update_mutex;    // for multithread support

inline int maxi(int a, int b)
{
	return (a > b) ? a : b;
}

inline float maxf(float a, float b) 
{
	return (a > b) ? a : b;
}

inline float minf(float a, float b)
{
	return (a < b) ? a : b;
}

inline float absf(float a)
{
	if (a < 0)
		return -a;
	return a;
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <main_module_name>\n", argv[0]);
		DR_ASSERT(false);
	}
	strncpy(main_module_name, argv[1], maxi(strlen(argv[1]), DIM_MODULE_NAME));
	main_module_name[DIM_MODULE_NAME-1] = '\0';

	dr_set_client_name("DynamoRIO Client 'fpvals'", "http://dynamorio.org");
	if (!drmgr_init())
		DR_ASSERT(false);
	dr_register_exit_event(exit_event);
	if (!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL))
		DR_ASSERT(false);

	update_mutex = dr_mutex_create();
	my_id = id;

	drmgr_register_thread_init_event(event_thread_init);
	drmgr_register_thread_exit_event(event_thread_exit);

#ifdef SHOW_SYMBOLS
	if (drsym_init(0) != DRSYM_SUCCESS)
	{
		dr_log(NULL, DR_LOG_ALL, 1, "WARNING: unable to initialize symbol translation\n");
	}
#endif
	tls_idx = drmgr_register_tls_field();
	DR_ASSERT(tls_idx > -1);
}

static void
exit_event(void)
{
#ifdef SHOW_RESULTS
	char msg[512];
	int len;
	len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]),
			"Instrumentation results:\n"
			"  minimum value in (-inf,-1]: %e\n"
			"  maximum value in    [-1,0): %e\n"
			"  minimum value in     (0,1]: %e\n"
			"  maximum value in  [1,+inf): %e\n",
			min_sup, max_sub, min_sub, max_sup);
	DR_ASSERT(len > 0);
	NULL_TERMINATE(msg);
	DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */

	dr_mutex_destroy(update_mutex);
	drmgr_unregister_tls_field(tls_idx);
	drmgr_exit();
}

static void
event_thread_init(void *drcontext)
{
	file_t f = log_file_open(my_id, drcontext, NULL /* client lib path */, "fpvals",
#ifndef WINDOWS
			DR_FILE_CLOSE_ON_FORK |
#endif
			DR_FILE_ALLOW_LARGE);
	DR_ASSERT(f != INVALID_FILE);

	/* store it in the slot provided in the drcontext */
	drmgr_set_tls_field(drcontext, tls_idx, (void *)(ptr_uint_t)f);
}

static void
event_thread_exit(void *drcontext)
{
	log_file_close((file_t)(ptr_uint_t)drmgr_get_tls_field(drcontext, tls_idx));
}

static bool
instr_is_fp(instr_t *instr) 
{
	return instr_is_floating(instr);
}

static void update_values(float valf)
{
	min_sup = minf(min_sup, valf);
	max_sup = maxf(max_sup, valf);
	if (valf < 0.0)
		max_sub = maxf(max_sub, valf);
	if (valf > 0.0)
		min_sub = minf(min_sub, valf);

}

static void
callback_fp_reg_1(app_pc drcontext_ptr, app_pc reg_id_val)
{
	file_t f = (file_t)(ptr_uint_t)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);

	void* drcontext = (void*)drcontext_ptr;
	reg_id_t reg_id = (reg_id_t)reg_id_val;
	reg_t reg_val = dr_read_saved_reg(drcontext, SPILL_SLOT_11);
	float valf;
	*((unsigned long int*)&valf) = reg_val;

	dr_fprintf(f, "Reg %d %lx %f\n", reg_id, reg_val, valf);

	dr_mutex_lock(update_mutex);
	update_values(valf);
	dr_mutex_unlock(update_mutex);
}

static void
callback_fp_mem_1(app_pc drcontext_ptr, app_pc mem_ptr_val)
{
	file_t f = (file_t)(ptr_uint_t)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);

	void* mem_ptr = (void*)mem_ptr_val;

	float valf;
	size_t nbr;
	if (!dr_safe_read(mem_ptr, sizeof(float), &valf, &nbr))
		return;

	dr_fprintf(f, "Mem x x %f\n", valf);

	dr_mutex_lock(update_mutex);
	update_values(valf);
	dr_mutex_unlock(update_mutex);
}

static void
callback_fp_reg_2(app_pc drcontext_ptr, app_pc reg1_id_val, app_pc reg2_id_val)
{
	file_t f = (file_t)(ptr_uint_t)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);

	void* drcontext = (void*)drcontext_ptr;
	reg_id_t reg1_id = (reg_id_t)reg1_id_val;
	reg_id_t reg2_id = (reg_id_t)reg2_id_val;
	reg_t reg1_val = dr_read_saved_reg(drcontext, SPILL_SLOT_11);
	reg_t reg2_val = dr_read_saved_reg(drcontext, SPILL_SLOT_12);
	float valf1, valf2;
	*((unsigned long int*)&valf1) = reg1_val;
	*((unsigned long int*)&valf2) = reg2_val;

	dr_fprintf(f, "Reg1 %d %lx %f\nReg2 %d %lx %f\n",
			reg1_id, reg1_val, valf1,
			reg2_id, reg2_val, valf2);

	dr_mutex_lock(update_mutex);
	update_values(valf1);
	update_values(valf2);
	dr_mutex_unlock(update_mutex);
}

static void
callback_fp_mem_2(app_pc drcontext_ptr, app_pc mem_ptr_val1, app_pc mem_ptr_val2)
{
	file_t f = (file_t)(ptr_uint_t)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);

	void* mem_ptr1 = (void*)mem_ptr_val1;
	void* mem_ptr2 = (void*)mem_ptr_val2;

	float valf1, valf2;
	size_t nbr;

	if (dr_safe_read(mem_ptr1, sizeof(float), &valf1, &nbr))
	{
		dr_fprintf(f, "Mem1 x x %f\n", valf1);
		dr_mutex_lock(update_mutex);
		update_values(valf1);
		dr_mutex_unlock(update_mutex);
	}

	if (dr_safe_read(mem_ptr2, sizeof(float), &valf2, &nbr))
	{
		dr_fprintf(f, "Mem2 x x %f\n", valf2);
		dr_mutex_lock(update_mutex);
		update_values(valf2);
		dr_mutex_unlock(update_mutex);
	}
}

static void
callback_fp_reg_3(app_pc drcontext_ptr, app_pc reg1_id_val, app_pc reg2_id_val, app_pc reg3_id_val)
{
	file_t f = (file_t)(ptr_uint_t)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);

	void* drcontext = (void*)drcontext_ptr;
	reg_id_t reg1_id = (reg_id_t)reg1_id_val;
	reg_id_t reg2_id = (reg_id_t)reg2_id_val;
	reg_id_t reg3_id = (reg_id_t)reg3_id_val;
	reg_t reg1_val = dr_read_saved_reg(drcontext, SPILL_SLOT_11);
	reg_t reg2_val = dr_read_saved_reg(drcontext, SPILL_SLOT_12);
	reg_t reg3_val = dr_read_saved_reg(drcontext, SPILL_SLOT_13);
	float valf1, valf2, valf3;
	*((unsigned long int*)&valf1) = reg1_val;
	*((unsigned long int*)&valf2) = reg2_val;
	*((unsigned long int*)&valf3) = reg3_val;

	dr_fprintf(f, "Reg1 %d %lx %f\nReg2 %d %lx %f\nReg3 %d %lx %f\n",
			reg1_id, reg1_val, valf1,
			reg2_id, reg2_val, valf2,
			reg3_id, reg3_val, valf3);

	dr_mutex_lock(update_mutex);
	update_values(valf1);
	update_values(valf2);
	update_values(valf3);
	dr_mutex_unlock(update_mutex);
}

static void
callback_fp_mem_3(app_pc drcontext_ptr, app_pc mem_ptr_val1, app_pc mem_ptr_val2, app_pc mem_ptr_val3)
{
	file_t f = (file_t)(ptr_uint_t)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);

	void* mem_ptr1 = (void*)mem_ptr_val1;
	void* mem_ptr2 = (void*)mem_ptr_val2;
	void* mem_ptr3 = (void*)mem_ptr_val3;

	float valf1, valf2, valf3;
	size_t nbr;

	if (dr_safe_read(mem_ptr1, sizeof(float), &valf1, &nbr))
	{
		dr_fprintf(f, "Mem1 x x %f\n", valf1);
		dr_mutex_lock(update_mutex);
		update_values(valf1);
		dr_mutex_unlock(update_mutex);
	}

	if (dr_safe_read(mem_ptr2, sizeof(float), &valf2, &nbr))
	{
		dr_fprintf(f, "Mem2 x x %f\n", valf2);
		dr_mutex_lock(update_mutex);
		update_values(valf2);
		dr_mutex_unlock(update_mutex);
	}

	if (dr_safe_read(mem_ptr3, sizeof(float), &valf3, &nbr))
	{
		dr_fprintf(f, "Mem3 x x %f\n", valf3);
		dr_mutex_lock(update_mutex);
		update_values(valf3);
		dr_mutex_unlock(update_mutex);
	}
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
		bool for_trace, bool translating, void *user_data)
{
	module_data_t* module_data = dr_lookup_module(instr_get_app_pc(instr));
	if (module_data == NULL)
		return DR_EMIT_DEFAULT;

	if (strstr(main_module_name, dr_module_preferred_name(module_data)) == NULL)
	{
		dr_free_module_data(module_data);
		return DR_EMIT_DEFAULT;
	}

	reg_id_t reg_ids[3];
	void* mem_ptrs[3];
	int n_regs = 0;
	int n_mems = 0;
	bool found = false;

	if (instr_is_fp(instr))
	{
		/*
		int dsts = instr_num_dsts(instr);
		for (int j = 0; j < dsts; j++)
		{
			opnd_t dst = instr_get_dst(instr, j);
			if (opnd_is_reg(dst))
			{
				found = true;
				reg_ids[n_regs] = opnd_get_reg(dst);
				n_regs++;
			}
			if (opnd_is_memory_reference(dst))
			{
				found = true;
				mem_ptrs[n_mems] = opnd_get_addr(dst);
				n_mems++;
			}
		}
		*/
		int srcs = instr_num_srcs(instr);
		for (int j = 0; j < srcs; j++)
		{
			opnd_t src = instr_get_src(instr, j);
			if (n_regs < 3 && opnd_is_reg(src) && reg_is_pointer_sized(opnd_get_reg(src)))
			{
				found = true;
				reg_ids[n_regs] = opnd_get_reg(src);
				n_regs++;
			}
			if (n_mems < 3 && opnd_is_memory_reference(src) && opnd_get_addr(src) != NULL)
			{
				found = true;
				mem_ptrs[n_mems] = opnd_get_addr(src);
				n_mems++;
			}
		}

		if (found)
		{
			instr = instr_get_next(instr);
			switch (n_regs)
			{
			case 0:
				break;
			case 1:
				dr_save_reg(drcontext, bb, instr, reg_ids[0], SPILL_SLOT_11);
				dr_insert_clean_call(drcontext, bb, instr, (void *)callback_fp_reg_1, true /* fp save */,
						2, OPND_CREATE_INTPTR(drcontext),
						OPND_CREATE_INTPTR(reg_ids[0]));
				break;
			case 2:
				dr_save_reg(drcontext, bb, instr, reg_ids[0], SPILL_SLOT_11);
				dr_save_reg(drcontext, bb, instr, reg_ids[1], SPILL_SLOT_12);
				dr_insert_clean_call(drcontext, bb, instr, (void *)callback_fp_reg_2, true /* fp save */,
						3, OPND_CREATE_INTPTR(drcontext),
						OPND_CREATE_INTPTR(reg_ids[0]),
						OPND_CREATE_INTPTR(reg_ids[2]));
				break;
			case 3:
				dr_save_reg(drcontext, bb, instr, reg_ids[0], SPILL_SLOT_11);
				dr_save_reg(drcontext, bb, instr, reg_ids[1], SPILL_SLOT_12);
				dr_save_reg(drcontext, bb, instr, reg_ids[2], SPILL_SLOT_13);
				dr_insert_clean_call(drcontext, bb, instr, (void *)callback_fp_reg_3, true /* fp save */,
						4, OPND_CREATE_INTPTR(drcontext),
						OPND_CREATE_INTPTR(reg_ids[0]),
						OPND_CREATE_INTPTR(reg_ids[1]),
						OPND_CREATE_INTPTR(reg_ids[2]));
				break;
			default:
				fprintf(stderr, "Unsupported number of registers: %d\n", n_regs);
			}

			switch (n_mems)
			{
			case 0:
				break;
			case 1:
				dr_insert_clean_call(drcontext, bb, instr, (void *)callback_fp_mem_1, true /* fp save */,
						2, OPND_CREATE_INTPTR(drcontext),
						OPND_CREATE_INTPTR(mem_ptrs[0]));
				break;
			case 2:
				dr_insert_clean_call(drcontext, bb, instr, (void *)callback_fp_mem_2, true /* fp save */,
						3, OPND_CREATE_INTPTR(drcontext),
						OPND_CREATE_INTPTR(mem_ptrs[0]),
						OPND_CREATE_INTPTR(mem_ptrs[2]));
				break;
			case 3:
				dr_insert_clean_call(drcontext, bb, instr, (void *)callback_fp_mem_3, true /* fp save */,
						4, OPND_CREATE_INTPTR(drcontext),
						OPND_CREATE_INTPTR(mem_ptrs[0]),
						OPND_CREATE_INTPTR(mem_ptrs[1]),
						OPND_CREATE_INTPTR(mem_ptrs[2]));
				break;
			default:
				fprintf(stderr, "Unsupported number of memory locations: %d\n", n_mems);
			}
		}
	}
	return DR_EMIT_DEFAULT;
}
