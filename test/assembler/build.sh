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
    asm6809 -3 -l $lst $i -o $DIR/$bin
done

for i in `ls *.bas`
do
    echo $i
    bin=${i%.bas}
    bin=${bin^^}
    basictool -t -r $i > $DIR/$bin
done

rm -f build.ssd

beeb blank_ssd build.ssd
beeb putfile build.ssd build/*
beeb info build.ssd
