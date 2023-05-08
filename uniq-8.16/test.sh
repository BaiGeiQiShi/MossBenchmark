#!/bin/bash -x

PROGNAME=mkdir-5.2.1
TIMEOUT=0.5
#GDTBIN="python3 /home/qxin6/ROPgadget/ROPgadget.py"
GDTBIN=ROPgadget
INPUTROOTDIR=$(pwd)

CURRDIR=$(pwd)
SRC=$CURRDIR/$PROGNAME.tmp.c
BIN=$CURRDIR/$PROGNAME
OUTDIR=$CURRDIR/output
INDIR=$INPUTROOTDIR/testscript
BASE_SRC=$CURRDIR/$PROGNAME.c.base.origin.c
BASE_BIN=$CURRDIR/$PROGNAME.base.origin
ORIGIN_SRC=$CURRDIR/$PROGNAME.c.cov.origin.c
ORIGIN_BIN=$CURRDIR/$PROGNAME.cov.origin
ORIGIN_OUTDIR=$CURRDIR/output.origin
DOMGAD=/usr/local/Moss/CovPath/
debdce=/usr/local/debdce/build/bin/debdce

clearProgram(){
 cwtty=$(tty)
 ps -a | grep ${cwtty:5} | grep $PROGNAME | xargs -n 1 kill -s 9
}
#Reset file content (original size; reduced size; original gadgets; reduced gadgets; total tests; passed tests)
echo "-1" > eval_rslt.txt
echo "-1" >> eval_rslt.txt
echo "-1" >> eval_rslt.txt
echo "-1" >> eval_rslt.txt
echo "-1" >> eval_rslt.txt
echo "-1" >> eval_rslt.txt

if [ ! -f ${BASE_BIN} ]; then
   cp ${BASE_SRC} $SRC
   ./compile.sh $SRC $BASE_BIN "-O3 -w"
fi
#Generate Oracle Bin (if needed)
if [ ! -f ${ORIGIN_BIN} ]; then
    cp ${ORIGIN_SRC} $SRC
    ./compile.sh ${SRC} ${ORIGIN_BIN} "-O3 -w"
fi
#Generate Oracle Outputs (if needed)
if [ ! -d ${ORIGIN_OUTDIR} ]; then
    mkdir ${ORIGIN_OUTDIR}
    cp ${ORIGIN_SRC} $SRC
    ./compile.sh ${SRC} ${BIN} "-O3 -w"                    #NOTE: Do NOT use ORIGIN_BIN here!
    ./run_test ${BIN} ${ORIGIN_OUTDIR} $TIMEOUT $INDIR #NOTE: Do NOT use ORIGIN_BIN here!
    rm ${BIN}
fi 
clearProgram

#Dead Code Eliminate
cp $CURRDIR/$PROGNAME.c $SRC

inputfname=$(basename $SRC)
if [ -d debdcetmp ]; then
    rm -rf debdcetmp/*
else
    mkdir debdcetmp
fi
cp $SRC debdcetmp/$inputfname
cd debdcetmp
$debdce debdcetest.sh $inputfname

cd ..
mv debdcetmp/$inputfname.dce.c $SRC
rm -rf debdcetmp


#Compile Reduced Program
./compile.sh $SRC $BIN "-O3 -w"

#Compute Size Reduction
#original_size=$(($(ls -l ${ORIGIN_BIN} | cut -d' ' -f5)-$(ls -l ${BASE_BIN} | cut -d' ' -f5)))
#reduced_size=$((`ls -l ${BIN} | cut -d' ' -f5` - `ls -l ${BASE_BIN} | cut -d' ' -f5`))
original_size=`$DOMGAD/build/bin/instrumenter -S ${ORIGIN_SRC}`
reduced_size=`$DOMGAD/build/bin/instrumenter -S ${SRC}`
#original_size=$(ls -l ${ORIGIN_BIN} | cut -d' ' -f5)
#reduced_size=$(ls -l ${BIN} | cut -d' ' -f5)


#Compute Gadget Reduction
original_gdt=`${GDTBIN} --binary ${ORIGIN_BIN} | grep 'Unique gadgets' | cut -d' ' -f4`
reduced_gdt=`${GDTBIN} --binary ${BIN} | grep 'Unique gadgets' | cut -d' ' -f4`

#Compute Generality
./run_test $BIN $OUTDIR $TIMEOUT $INDIR
./compare_output ${ORIGIN_OUTDIR} $OUTDIR $CURRDIR/compare.txt
clearProgram

#total_ukn=$(ls $CURRDIR/testscript/ukn | wc -l) #total ukn inputs
pass_all=`grep 'pass-' $CURRDIR/compare.txt | wc -l`
total_all=$(ls $CURRDIR/testscript/kn | wc -l) #total (all=kn) inputs
#rm $CURRDIR/compare.txt
#mv compare.txt compare-$(date +%H:%M:%S).txt


#Output to file
echo "${original_size}" > eval_rslt.txt
echo "${reduced_size}" >> eval_rslt.txt
echo "${original_gdt}" >> eval_rslt.txt
echo "${reduced_gdt}" >> eval_rslt.txt
echo "${total_all}" >> eval_rslt.txt
echo "${pass_all}" >> eval_rslt.txt
