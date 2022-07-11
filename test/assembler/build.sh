#!/bin/bash

DIR=build

mkdir -p $DIR
rm -f $DIR/*

for i in `ls *.asm`
do
    echo $i
    bin=${i%.asm}
    bin=${bin^^}
    lst=${bin}.lst
    asm6809 -l $lst $i > $DIR/$bin
done

for i in `ls *.bas`
do
    echo $i
    bin=${i%.bas}
    bin=${bin^^}
    basictool -t -r $i > $DIR/$bin
done
