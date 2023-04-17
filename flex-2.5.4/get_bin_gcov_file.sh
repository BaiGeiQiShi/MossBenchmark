#!/bin/bash

PROGRAM=$1
COV=$2
LCOV2GCOV_BIN=${COV}/bin/lcov2gcov
GCOV_ANAL_BIN=${COV}/bin/gcovanalyzer

llvm-profdata merge -o $PROGRAM.profdata default.profraw
llvm-cov export -format=lcov ./$PROGRAM -instr-profile=$PROGRAM.profdata $PROGRAM.c >$PROGRAM.lcov

$LCOV2GCOV_BIN $PROGRAM.lcov >$PROGRAM.real.gcov
$GCOV_ANAL_BIN $PROGRAM.real.gcov getbcov >$PROGRAM.bin.gcov
