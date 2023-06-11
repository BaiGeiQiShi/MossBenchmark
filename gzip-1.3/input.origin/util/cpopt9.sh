#!/bin/bash

INDIR=$1
OUTFILE=$2

if [ -f $INDIR/testdir/file22.gz ]
then
  cp $INDIR/testdir/file22.gz $OUTFILE
else
  cp $INDIR/testdir/file22.z $OUTFILE
fi

#$INDIR/util/cleanup.sh $INDIR
