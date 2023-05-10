#!/bin/bash 


CURRDIR=$(pwd)
PROGRAM=bzip2-1.0.5
DOMGAD=/usr/local/domgad
COV=/usr/local/cov		 #change to your own path
DEBOP=/usr/local/debop           #change to your own path
kr=0.5              #Weight balancing size red & attksurf red
kg=0.5               #Weight balancing red & gen
kvalue=50           #k for computing density score
domgad_samplenum=1000
debop_samplenum=1000
debop_iternum=100000000
realorcov=0 #1 means real, others mean cov
#version=1 means basicblock level, 0 means statement level

label=real
if [ "${realorcov}" -eq "1" ]; then
    label=real
else
    label=cov
fi

LINEPRINTERBIN="${DOMGAD}/build/bin/instrumenter -g statement test.sh"
SEARCHBIN="java -cp :${DOMGAD}/build/java:${DOMGAD}/lib/java/* edu.gatech.cc.domgad.GCovBasedMCMCSearch"

#0.Generate the line.txt
$LINEPRINTERBIN $CURRDIR/$PROGRAM.c.real.origin.c >$CURRDIR/path_generator/line.txt

#1.Generate the cov.c file and path file
$CURRDIR/path_generator/generate_cov.sh $PROGRAM $COV

#2.CP the path file to identify_path and quantify_path
if [ ! -d identify_path ]; then
    mkdir identify_path
else
    chmod 755 -R identify_path
    rm -fr identify_path/*
fi

for testf in $CURRDIR/tmp/bin.gcov/*
do
        test_name=${testf##*_}
        path_name=${test_name%%.*}
	cp ${testf} $CURRDIR/identify_path/${path_name}
done

#Rm the quantify_path
if [ -d quantify_path ]; then
    rm -fr quantify_path
fi

cp -r $CURRDIR/identify_path $CURRDIR/quantify_path

#3.Generate the path_counted.txt
>$CURRDIR/tmp/path_counted.txt
pid=0
for ippath in identify_path/*; do
    ipid=$(basename ${ippath})
    count=1
    qpids=${ipid}

    echo "${pid},${ipid},${count},${qpids}" >>$CURRDIR/tmp/path_counted.txt
    pid=$((pid+1))
done

#4.Run Domgad Stochasticsearch.sh
quan_num=`ls $CURRDIR/quantify_path | wc -l`
#echo ${quan_num} >$CURRDIR/tmp/quan_num.txt

#cp $CURRDIR/getsize.sh $CURRDIR/tmp/getsize.sh
#cp $CURRDIR/path_generator/compile_for_domgad.sh $CURRDIR/tmp/compile.sh

#$SEARCHBIN $CURRDIR/tmp/path_counted.txt $CURRDIR/identify_path $CURRDIR/tmp/sample_output ${domgad_samplenum} $CURRDIR/tmp/${PROGRAM}.c $CURRDIR/tmp/line.txt $CURRDIR/tmp $PROGRAM ${kr} ${kg} ${kvalue} ${quan_num} >$CURRDIR/log/mcmclog.txt
