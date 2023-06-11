#!/bin/bash

SRC=$1
BIN=$2
flags=$3

flags="${flags} -L/MossBenchmark/bash-2.05/lib"

if [ -z $4 ]; then
    COMPILER="clang" #Default
else
    COMPILER=$4
fi
$COMPILER $SRC ${flags} -w -o $BIN -lbuiltins -lsh -lreadline -lhistory -ltermcap -lglob -ltilde -lmalloc  -ldl
