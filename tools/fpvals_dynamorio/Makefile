DRROOT := /home/dumi/git/riscv/DynamoRIO-Linux-8.0.0-1
#DRROOT := /home/dumi/git/riscv/dynamorio/build
MODE := release
#MODE := debug

CC := gcc

C_FLAGS = -fPIC -DX86_64 -DLINUX -DDR_HOST_X86 -DDR_HOST_X64 -DUSE_VISIBILITY_ATTRIBUTES

C_DEFINES = -DSHOW_RESULTS -DSHOW_SYMBOLS -Ddiv_EXPORTS

I_INC1 = $(DRROOT)/include
I_INC2 = $(DRROOT)/ext/include

C_INCLUDES = -I$(I_INC1) -I$(I_INC2)

L_LIB1 = $(DRROOT)/lib64/$(MODE)
L_LIB2 = $(DRROOT)/lib64
L_LIB3 = $(DRROOT)/ext/lib64/$(MODE)


all: fpvals.so

fpvals.o: fpvals.c
	$(CC) $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -m64 -fno-strict-aliasing -fno-stack-protector -fvisibility=internal -std=gnu99 -fno-unwind-tables -Wall -Wwrite-strings -Wno-unused-but-set-variable -Wno-stringop-truncation -Wno-format-truncation -Wno-stringop-overflow -O2 -fno-stack-protector -o $@ -c $^

utils.o: utils.c
	$(CC) $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -m64 -fno-strict-aliasing -fno-stack-protector -fvisibility=internal -std=gnu99 -fno-unwind-tables -Wall -Wwrite-strings -Wno-unused-but-set-variable -Wno-stringop-truncation -Wno-format-truncation -Wno-stringop-overflow -O2 -fno-stack-protector -o $@ -c $^
  
fpvals.so: utils.o fpvals.o
	$(CC) -fPIC -m64 -fno-strict-aliasing -fno-stack-protector -fvisibility=internal  -std=gnu99 -fno-unwind-tables -Wall -Wwrite-strings -Wno-unused-but-set-variable -Wno-stringop-truncation -Wno-format-truncation -Wno-stringop-overflow -DX86_64 -DLINUX -DDR_HOST_X86 -DDR_HOST_X64 -DUSE_VISIBILITY_ATTRIBUTES -fno-stack-protector  -Xlinker -z -Xlinker now -fPIC -shared -lgcc -Wl,--hash-style=both -Xlinker -T -Xlinker div.ldscript -Wl,--hash-style=both -shared -Wl,-soname,$@ -o $@ $^ $(DRROOT)/ext/lib64/$(MODE)/libdrsyms.so $(DRROOT)/ext/lib64/$(MODE)/libdrx.so $(DRROOT)/ext/lib64/$(MODE)/libdrreg.so $(DRROOT)/ext/lib64/$(MODE)/libdrmgr.so $(DRROOT)/ext/lib64/$(MODE)/libdrcontainers.a $(DRROOT)/lib64/$(MODE)/libdynamorio.so

clean:
	rm -f utils.o fpvals.o fpvals.so
