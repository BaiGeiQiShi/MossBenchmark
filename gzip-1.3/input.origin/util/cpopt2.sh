#!/bin/bash

INDIR=$1
OUTFILE=$2

if [ -f $INDIR/testdir/file15.gz ]
then
  cp $INDIR/testdir/file15.gz $OUTFILE
else
  cp $INDIR/testdir/file15.z $OUTFILE
fi

#$INDIR/util/cleanup.sh $INDIR
