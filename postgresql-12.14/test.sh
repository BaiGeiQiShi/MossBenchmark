#!/bin/bash 

PROGNAME=postgres
TIMEOUT=3
GDTBIN=ROPgadget

CURRDIR=$(pwd)
BINFOLDER=$CURRDIR/pgsql/bin/
OUTDIR=$CURRDIR/output
ORIGIN_OUTDIR=$CURRDIR/output.origin
EVALRESULTFILE=$CURRDIR/eval_rslt.txt

#Reset file content
echo "-1" > $EVALRESULTFILE
echo "-1" >> $EVALRESULTFILE
echo "-1" >> $EVALRESULTFILE
echo "-1" >> $EVALRESULTFILE
echo "-1" >> $EVALRESULTFILE
echo "-1" >> $EVALRESULTFILE

calculate_ar_sum() {
    local total_sum=0

    for bin in $BINFOLDER/*; do
        if [ -f $bin ]; then
        bin_ar=$( $GDTBIN --binary $bin | grep 'Unique gadgets' | cut -d' ' -f4 )
        total_sum=$((total_sum + bin_ar))
        fi
    done
    echo $total_sum
}


# origin
if [ -d originscore ]
then
    read -t 5 original_size original_gdt < originscore
else
    while IFS= read -r program; do
        mv $program $program.reduced.c
        cp $program.origin.c $program
    done < programlist

    #make && make install
    #find $CURRDIR/src -name "*.o" | xargs -n 1 rm 
    chown -R "postgres" $CURRDIR/src
    su postgres -m -c "make > /dev/null"
    su postgres -m -c "make install > /dev/null"
    original_size=$(du -s $BINFOLDER | cut -f 1)
    original_gdt=$(calculate_ar_sum)

    echo "$original_size $original_gdt" > originscore

    if [ ! -d ${ORIGIN_OUTDIR} ]; then
        mkdir -m 777 ${ORIGIN_OUTDIR}
        su postgres -c ./generate_sql.py
        ./run_test $BINFOLDER ${ORIGIN_OUTDIR} $TIMEOUT $CURRDIR
	./cleanup
    fi
fi


# reduced
while IFS= read -r program; do
    cp $program.reduced.c $program
done < programlist

#find $CURRDIR/src -name "*.o" | xargs -n 1 rm 
chown -R "postgres" $CURRDIR/src
su postgres -m -c "make > /dev/null"
su postgres -m -c "make install > /dev/null"
reduced_size=$(du -s $BINFOLDER | cut -f 1)
reduced_gdt=$(calculate_ar_sum)

rm -rf $OUTDIR
mkdir -m 777 $OUTDIR
su postgres -c ./generate_sql.py
./run_test $BINFOLDER $OUTDIR $TIMEOUT $CURRDIR
./cleanup

#Compute Generality (gen)
./compare_output ${ORIGIN_OUTDIR} $OUTDIR $CURRDIR/compare.txt
pass_all=`grep 'pass-' $CURRDIR/compare.txt | wc -l`
total_all=$(ls $CURRDIR/testscript/kn | wc -l)
#rm $CURRDIR/compare.txt

echo "${original_size}" > $EVALRESULTFILE
echo "${reduced_size}" >> $EVALRESULTFILE
echo "${original_gdt}" >> $EVALRESULTFILE
echo "${reduced_gdt}" >> $EVALRESULTFILE
echo "${total_all}" >> $EVALRESULTFILE
echo "${pass_all}" >> $EVALRESULTFILE
