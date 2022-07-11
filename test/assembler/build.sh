#!/bin/bash

DIR=build

mkdir -p $DIR
rm -f $DIR/*

for i in `ls *.asm`
do
    echo $i
    bin=${i%.asm}
    bin=${bin^^}
    basictool -t -r $i > $DIR/$bin
done
