#!/bin/bash 

PROGRAM=$1
COV=$2
WORKINGPATH=$3
LCOV2GCOV_BIN=${COV}/bin/lcov2gcov
GCOV_ANAL_BIN=${COV}/bin/gcovanalyzer
PROGNAME=${PROGRAM##*/}

llvm-profdata merge -o $WORKINGPATH/$PROGNAME.profdata default.profraw
llvm-cov export -format=lcov $PROGRAM -instr-profile=$WORKINGPATH/$PROGNAME.profdata $PROGRAM.c > $WORKINGPATH/$PROGNAME.lcov

$LCOV2GCOV_BIN $WORKINGPATH/$PROGNAME.lcov > $WORKINGPATH/$PROGNAME.real.gcov
$GCOV_ANAL_BIN $WORKINGPATH/$PROGNAME.real.gcov getbcov > $WORKINGPATH/$PROGNAME.bin.gcov
