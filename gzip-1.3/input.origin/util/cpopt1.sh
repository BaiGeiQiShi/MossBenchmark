#!/bin/bash

INDIR=$1
OUTFILE=$2

if [ -f $INDIR/testdir/file13.gz ]
then
  cp $INDIR/testdir/file13.gz $OUTFILE || { echo "$0: cp to output dir failed" >>$OUTFILE; }
else
  cp $INDIR/testdir/file13.z $OUTFILE || echo "$0: cp to output dir failed" >>$OUTFILE;
fi

#$INDIR/util/cleanup.sh $INDIR
