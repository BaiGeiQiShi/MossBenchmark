#!/bin/bash -x
PROGNAME=$1
COV=$2
#generate base-input program
mv testscript/kn testscript/kn.bck
mkdir testscript/kn
head -n 1 BaseInputs.txt | awk -F@ '{print $2}' | sed 's/,/\n/g' | xargs -n 1 -I{} cp testscript/kn.bck/test_{} testscript/kn
path_generator/generate_cov.py $PROGNAME $COV
rm -rf testscript/kn
mv testscript/kn.bck testscript/kn
mv $PROGNAME.c.cov.origin.c $PROGNAME.c.base.origin.c
