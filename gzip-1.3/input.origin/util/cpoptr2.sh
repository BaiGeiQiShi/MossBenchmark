#!/bin/bash

INDIR=$1
OUTFILE=$2

cp $INDIR/testdir/subdir3/file $OUTFILE

#$INDIR/util/cleanup.sh $INDIR