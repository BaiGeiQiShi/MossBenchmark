#!/bin/bash
:<<EOF
INDIR=$1

if [ -z $INDIR ]; then
    echo "Missing argument"
    exit 1
fi

rm -rf $INDIR/CONTENTS $INDIR/gzdir $INDIR/testdir
cp -r $INDIR/inputbackup $INDIR/ || { echo "$0: Cannot replenish inputs directory"; exit 1; }
chmod -R u+w $INDIR

chmod 000 $INDIR/testdir/nopermissionfile || { echo "$0: Cannot change permission for nopermissionfile"; exit 1; }
EOF