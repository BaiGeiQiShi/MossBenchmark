#!/bin/bash

CURRDIR=$(pwd)
PROGRAM=date-8.21
DOMGAD=/usr/local/domgad
COV=/usr/local/cov               #change to your own path
DEBOP=/usr/local/debop           #change to your own path
kr=0.5              #Weight balancing size red & attksurf red
kg=0.5               #Weight balancing red & gen
kvalue=50           #k for computing density score
domgad_samplenum=5
debop_samplenum=5
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

