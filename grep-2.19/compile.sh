#!/bin/bash

SRC=$1
BIN=$2

clang $3 -w -o $BIN $SRC "-D__msan_unpoison(s,z)" -lpcre
