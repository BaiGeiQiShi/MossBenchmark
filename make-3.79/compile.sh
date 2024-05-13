#!/bin/bash

SRC=$1
BIN=$2
flags=$3
libglob_path=/MossBenchmark/make-3.79/lib/libglob.a
if [ -z $4 ]; then
    COMPILER=clang #Default
else
    COMPILER=$4
fi

$COMPILER ${flags} -w -o $BIN $SRC ${libglob_path} -lutil
