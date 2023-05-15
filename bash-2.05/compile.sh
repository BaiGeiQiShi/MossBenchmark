#!/bin/bash -x

SRC=$1
BIN=$2
flags=$3

flags="${flags} -L/moss-script/moss/bash-2.05/lib"

if [ -z $4 ]; then
    COMPILER="clang" #Default
else
    COMPILER=$4
fi
date
$COMPILER -c -w $SRC -o $SRC.o ${flags}
$COMPILER ${flags} -w -o $BIN $SRC.o -lbuiltins -lsh -lreadline -lhistory -ltermcap -lglob -ltilde -lmalloc  -ldl
date
