#!/bin/bash

INDIR=$1
OUTFILE=$2

if [ -f $INDIR/testdir/file11.gz ]
then
  cp $INDIR/testdir/file11.gz $OUTFILE
else
  cp $INDIR/testdir/file11.z $OUTFILE
fi

#$INDIR/util/cleanup.sh $INDIR
