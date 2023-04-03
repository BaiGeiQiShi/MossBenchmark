#!/bin/bash -x

PROGNAME=mkdir-5.2.1
TIMEOUT=1
GDTBIN=ROPgadget
DOMGAD=/usr/local/Moss/CovPath
#PROG_COUNTER_BIN="/home/qxin6/progcounter/build/bin/counter"
PROG_COUNTER_BIN="${DOMGAD}/build/bin/instrumenter -S"
debdce=/usr/local/debdce/build/bin/debdce
WORKDIR=$(pwd)
SRC=$WORKDIR/$PROGNAME.tmp.c
#ORIGIN_SRC=$WORKDIR/$PROGNAME.c.real.origin.c
ORIGIN_SRC=$WORKDIR/$PROGNAME.c.cov.origin.c
BIN=$WORKDIR/$PROGNAME.reduced
ORIGIN_BIN=$WORKDIR/$PROGNAME.origin
BASE_SRC=$WORKDIR/$PROGNAME.c.base.origin.c
BASE_BIN=$WORKDIR/$PROGNAME.base
#Reset file content (original binary bytes; reduced binary bytes; original gadgets; reduced gadgets; original #stmts; reduced #stmts)
echo "-1" > size_rslt.txt
echo "-1" >> size_rslt.txt
echo "-1" >> size_rslt.txt
echo "-1" >> size_rslt.txt
echo "-1" >> size_rslt.txt
echo "-1" >> size_rslt.txt

if [ ! -f ${BASE_BIN} ]; then
    ./compile.sh ${BASE_SRC} ${BASE_BIN} "-w -O3" || exit 1
fi

#Generate Oracle Bin (if needed)
if [ ! -f ${ORIGIN_BIN} ]; then
    ./compile.sh ${ORIGIN_SRC} ${ORIGIN_BIN} "-w -O3" || exit 1
fi

#Compile Reduced Program
#clang -w -o $BIN $SRC || exit 1
cp $WORKDIR/$PROGNAME.c.reduced.c $SRC
./compile.sh $SRC $BIN "-w -O3" || exit 1

#Count Binary Bytes
original_size=$((`ls -l ${ORIGIN_BIN} | cut -d' ' -f5`-`ls -l $BASE_BIN | cut -d' ' -f5`))
reduced_size=$((`ls -l ${BIN} | cut -d' ' -f5`-`ls -l $BASE_BIN | cut -d' ' -f5`))

#Count Gadgets
original_gdt=`${GDTBIN} --binary ${ORIGIN_BIN} | grep 'Unique gadgets' | cut -d' ' -f4`
reduced_gdt=`${GDTBIN} --binary ${BIN} | grep 'Unique gadgets' | cut -d' ' -f4`

#Count Stmts
original_stmts=-1
if [ -f ${WORKDIR}/original_stmt_num.txt ]; then
    original_stmts=`head -n 1 ${WORKDIR}/original_stmt_num.txt`
else
    original_stmts=`${PROG_COUNTER_BIN} ${ORIGIN_SRC}`
    echo ${original_stmts} >${WORKDIR}/original_stmt_num.txt
fi
reduced_stmts=`${PROG_COUNTER_BIN} ${SRC}`


#Output to file
echo "${original_size}" > size_rslt.txt
echo "${reduced_size}" >> size_rslt.txt
echo "${original_gdt}" >> size_rslt.txt
echo "${reduced_gdt}" >> size_rslt.txt
echo "${original_stmts}" >> size_rslt.txt
echo "${reduced_stmts}" >> size_rslt.txt
