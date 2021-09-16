# POSAR: A Flexible Posit Arithmetic Unit for RISC-V

This README will guide you through the building process of POSAR - a flexible POSit ARithmetic unit for Rocket Chip, a RISC-V core.

POSAR will be presented:

- at the 39th IEEE International Conference on Computer Design ([ICCD '21](https://www.iccd-conf.com/Home.html)) under the title *"The Accuracy and Efficiency of Posit Arithmetic"*
- at the Design Automation Conference ([DAC '21](https://www.dac.com/)) as a Work-in-Progress under the title *"POSAR: A Flexible Posit Arithmetic Unit for RISC-V"*
- as an extended paper on arxiv.

## POSAR Source Code

POSAR source code can be found [here](https://github.com/sdcioc/PositChisel).

The integration in Rocket Chip under the "freedom" framework can be found [here](https://github.com/dloghin/freedom).

## Rocket Chip with POSAR

Use this second repository to build the FPGA images for POSAR, as well as the original Rocket Chip with IEEE 754 floating-point unit (FPU):

```
$ git clone https://github.com/dloghin/freedom
$ cd freedom
// to make Rocket Chip + FPU
$ ./make.sh fp32
// to make Rocker Chip + 32-bit Posit (3-bit exponent)
$ ./make.sh posit32
// to make Rocker Chip + 16-bit Posit (2-bit exponent)
$ ./make.sh posit16
// to make Rocker Chip + 8-bit Posit (1-bit exponent)
$ ./make.sh posit8
```

To obtain a system with increased memory on Arty A7, please read my [blogpost](https://dloghin.medium.com/how-to-increase-the-size-of-the-data-memory-on-sifive-fe310-risc-v-f05df0f50a25) and use the following commands:

```
$ ./make.sh fp32xmem
// to make Rocker Chip + 32-bit Posit (3-bit exponent)
$ ./make.sh posit32xmem
// to make Rocker Chip + 16-bit Posit (2-bit exponent)
$ ./make.sh posit16xmem
// to make Rocker Chip + 8-bit Posit (1-bit exponent)
$ ./make.sh posit8xmem
```

To upload the bitstream on Arty A7 and debug a simple program, please follow [this tutorial](https://forum.digikey.com/t/digilent-arty-a7-with-xilinx-artix-7-implementing-sifive-fe310-risc-v/13311).

## Benchmarks

The benchmarks presented in the paper can be found [here](https://github.com/dloghin/freedom-e-sdk), in particular, under the [software](https://github.com/dloghin/freedom-e-sdk/tree/posit/software) folder.

To run/debug a benchmark (e.g., Euler number computation), first set your environment in ``env.sh``, and then run:

```
source env.sh
make PROGRAM=euler TARGET=freedom-e310-arty-64bit-fpu CONFIGURATION=debug clean
make PROGRAM=euler TARGET=freedom-e310-arty-64bit-fpu CONFIGURATION=debug software
make PROGRAM=euler TARGET=freedom-e310-arty-64bit-fpu CONFIGURATION=debug upload
```

# Authors

Stefan Dan Ciocirlan, Dumitrel Loghin, Lavanya Ramapantulu, Nicolae Tăpus, Yong Meng Teo
