#!/bin/bash

INDIR=$1
OUTFILE=$2

if [ -f $INDIR/testdir/file19.gz ]
then
  cp $INDIR/testdir/file19.gz $OUTFILE
else
  cp $INDIR/testdir/file19.z $OUTFILE
fi

#$INDIR/util/cleanup.sh $INDIR
