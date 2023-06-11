#!/bin/bash

INDIR=$1
OUTFILE=$2

if [ -f $INDIR/testdir/file21.gz ]
then
  cp $INDIR/testdir/file21.gz $OUTFILE
else
  cp $INDIR/testdir/file21.z $OUTFILE
fi

#$INDIR/util/cleanup.sh $INDIR
