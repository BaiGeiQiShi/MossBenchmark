#!/bin/bash
PROGRAM=$1
LLVMCBE=/usr/local/llvm-project/llvm/build/bin/llvm-cbe
LLVMDep=/usr/local/GetDependency/src/llvmDep
RegDep=/usr/local/GetDependency/src/getDependency.py
MergeDep=/usr/local/GetDependency/src/MergeDep.py
CURRDIR=$(pwd)

SRC=${PROGRAM}.c.cov.origin.c
IR=${CURRDIR}/tmp/${PROGRAM}.ll
if [ ! -d ${CURRDIR}/tmp ]
then
  mkdir $CURRDIR/tmp
fi

# get .ll file
clang -S -g -w -O0 -c -Xclang -disable-O0-optnone -fno-discard-value-names -emit-llvm $SRC -o ${CURRDIR}/tmp/${PROGRAM}.O0.ll || exit
opt -mem2reg ${CURRDIR}/tmp/${PROGRAM}.O0.ll -o $CURRDIR/tmp/${PROGRAM}_opt.ll || exit
llvm-dis $CURRDIR/tmp/${PROGRAM}_opt.ll -o $CURRDIR/tmp/${PROGRAM}.ll || exit

# get dep info from llvm def use
$LLVMDep $CURRDIR/tmp/$PROGRAM.ll 2>/dev/null 1>$CURRDIR/tmp/LLVMDEPINFO

# get .cbe.c
$LLVMCBE $IR -o $CURRDIR/tmp/$PROGRAM.cbe.c || exit
# get dep info from regex and cbe
python3 $RegDep $CURRDIR/tmp/$PROGRAM.cbe.c >$CURRDIR/tmp/REGDEPINFO

# merge this two infos together
python3 $MergeDep $CURRDIR/tmp/REGDEPINFO $CURRDIR/tmp/LLVMDEPINFO ${PROGRAM}.LineDepInfo
