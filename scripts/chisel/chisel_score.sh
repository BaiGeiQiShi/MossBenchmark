#!/bin/bash

PROG_NAME=$1

CURR_DIR=$(pwd)
TESTSET_DIR=$CURR_DIR/testscript
KN_DIR=$TESTSET_DIR/kn
FINAL_OUTDIR=$CURR_DIR/final_out

SCORE_REPO_CSV=$CURR_DIR/chisel_final_score.csv

if [ -z ${PROG_NAME} ]; then
	echo "Missing argument"
	exit 1
fi

if [ ! -d ${FINAL_OUTDIR} ] || [ -z $(ls ${FINAL_OUTDIR}/a0.*.b0.* | wc -l) ]; then
	echo "Missing final_out"	
	exit 1
fi

if [ ! -d ${CURR_DIR}/scores ]; then
	mkdir ${CURR_DIR}/scores
else
	rm -rf ${CURR_DIR}/scores/*
fi

> ${SCORE_REPO_CSV}

for input_set in ${FINAL_OUTDIR}/a0.*.b0.*; do
	set_name=$(basename ${input_set})
	score_file=$CURR_DIR/scores/oscore_${set_name}.repo
	> ${score_file}
	if [ -f $input_set/$PROG_NAME.c.chisel.c ]; then
		./chisel_oscore.sh ${PROG_NAME} ${set_name} $(realpath ${input_set}/${PROG_NAME}.c.chisel.c) ${score_file}
	else
		# for sourcefile in ${input_set}/*.c; do
		# 	./chisel_oscore.sh ${PROG_NAME} ${set_name} $(realpath ${sourcefile}) ${score_file}
        # done
        lastnumber=$(ls ${input_set}/*.c | grep -oE '.c\.[0-9]+\.' | sort -t'.' -n -r -k 3 | grep -oE '[0-9]+' | head -1)
        sourcefile=$(ls ${input_set}/*${lastnumber}*.c)
        ./chisel_oscore.sh ${PROG_NAME} ${set_name} $(realpath ${sourcefile}) ${score_file}
	fi
	best_score=$(sort -t',' -n -r -k 4 ${score_file} | head -1)
	echo "${set_name},${best_score}" >> ${SCORE_REPO_CSV}
done
