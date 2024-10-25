#!/bin/bash

PROGNAME=integration1
TIMEOUT=1
GDTBIN=ROPgadget
DOMGAD=/usr/local/Moss/multiFiles/CovPath
STMT_COUNTER="${DOMGAD}/build/bin/instrumenter -S"
debdce=/usr/local/Moss/debdce/build/bin/debdce
WORKDIR=$(pwd)
EVALRESULTFILE=$WORKDIR/size_rslt.txt
MAKEFILE=$WORKDIR/Makefile


BIN=$WORKDIR/$PROGNAME
ORIGIN_BIN=$WORKDIR/$PROGNAME.origin
BASE_BIN=$WORKDIR/$PROGNAME.base

BINFOLDER=$WORKDIR/pgsql/bin/

#Reset file content (original binary bytes; reduced binary bytes; original gadgets; reduced gadgets; original #stmts; reduced #stmts)
echo "-1" > $EVALRESULTFILE
echo "-1" >> $EVALRESULTFILE
echo "-1" >> $EVALRESULTFILE
echo "-1" >> $EVALRESULTFILE
echo "-1" >> $EVALRESULTFILE
echo "-1" >> $EVALRESULTFILE

calculate_ar_sum() {
    local total_sum=0

    for bin in $BINFOLDER/*; do
	basename_bin=$(basename $bin)
        if [ -f $bin ] && [ $basename_bin != "postmaster" ]; then
        bin_ar=$( $GDTBIN --binary $bin | grep 'Unique gadgets' | cut -d' ' -f4 )
        total_sum=$((total_sum + bin_ar))
        fi
    done
    echo $total_sum
}

#reduced
reduced_stmts=0
while IFS= read -r program; do
    statements_info=$($STMT_COUNTER $program)
    reduced_stmts=$((reduced_stmts + statements_info))
done < programlist

#make && make install
chown -R postgres $WORKDIR/src
su postgres -m -c "make > /dev/null && make install > /dev/null" || exit 1

reduced_size=$(du -s $BINFOLDER | cut -f 1)
reduced_gdt=$(calculate_ar_sum)

#origin
original_stmts=0
if [ -e originscore ]
then
    read -t 5 original_size original_gdt original_stmts < originscore
else
    while IFS= read -r program; do
        mv $program $program.reduced.c
        cp $program.origin.c $program
        statements_info=$($STMT_COUNTER $program.origin.c)
        original_stmts=$((original_stmts + statements_info))
    done < programlist

    #make && make install 
    chown -R postgres $WORKDIR/src
    su postgres -m -c "make > /dev/null && make install > /dev/null" || exit 1

    original_size=$(du -s $BINFOLDER | cut -f 1)
    original_gdt=$(calculate_ar_sum)

    echo "$original_size $original_gdt $original_stmts" > originscore

    #origin: get .c back to reduced file
    while IFS= read -r program; do
        mv $program.reduced.c $program
    done < programlist
fi

#Output to file
echo "${original_size}" > $EVALRESULTFILE
echo "${reduced_size}" >> $EVALRESULTFILE
echo "${original_gdt}" >> $EVALRESULTFILE
echo "${reduced_gdt}" >> $EVALRESULTFILE
echo "${original_stmts}" >> $EVALRESULTFILE
echo "${reduced_stmts}" >> $EVALRESULTFILE
