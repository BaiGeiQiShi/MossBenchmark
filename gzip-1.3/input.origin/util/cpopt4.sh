#!/bin/bash

INDIR=$1
OUTFILE=$2

if [ -f $INDIR/testdir/file17.gz ]
then
  cp $INDIR/testdir/file17.gz $OUTFILE
else
  cp $INDIR/testdir/file17.z $OUTFILE
fi

#$INDIR/util/cleanup.sh $INDIR
