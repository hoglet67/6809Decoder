#!/bin/bash

# Log all memory reads/writes/errors
# Log all samples
# Ignore the LIC input
# Output address, hex, instruction, state, cycles

COMMON="--mem=FFF -d1 --lic= -ahisy"

./posigrab posiBoot0xFFFE8k.fixed posiBoot0xFFFE8k.bin
./posigrab posiBootF105.txt posiBootF105.bin
./posigrab posiBootDA15.txt posiBootDA15.bin

../../decode6809 $COMMON                                         posiBoot0xFFFE8k.bin > posiBoot0xFFFE8k.log
../../decode6809 $COMMON --ba= --bs= --reg_pc=F105 --reg_s=0900      posiBootF105.bin >     posiBootF105.log
../../decode6809 $COMMON                                             posiBootDA15.bin >     posiBootDA15.log
