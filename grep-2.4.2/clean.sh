#!/bin/bash

#####
# Clean up the environment
#####

ARG_PROG_NAME=$1
ARG_TYPE=$2
ARG_REMOVE_RESULT=$3


if [ -z ${ARG_PROG_NAME} ]; then
    ARG_PROG_NAME=$(basename $(pwd))
fi

if [ -z ${ARG_REMOVE_RESULT} ]; then
    ARG_REMOVE_RESULT="false"
fi

if [ ${ARG_TYPE} = "razor" ]; then
    PROG_ABBR=$(echo ${ARG_PROG_NAME} | cut -d'-' -f1)

    if [ ${ARG_REMOVE_RESULT} = "true" ]; then
        rm -rf log_*.txt
        rm -rf razor_code
        rm -rf log
        rm -rf log_*txt
        rm -rf score*repo
    fi

    rm -rf callbacks.txt
    rm -rf instr.s
    rm -rf logs

    if [ ! -z ${PROG_ABBR} ]; then
            rm -rf ${PROG_ABBR}-trace.log
            rm -rf ${PROG_ABBR}-extended.log
            rm -rf ${PROG_ABBR}.orig.asm
            rm -rf ${PROG_ABBR}.orig_temp
            rm -rf ${PROG_ABBR}.s
    fi
fi 

if [ ${ARG_TYPE} = "chisel" ]; then
    rm -rf ${ARG_PROG_NAME}
    rm -rf ${ARG_PROG_NAME}.c
    rm -rf ${ARG_PROG_NAME}.tmp.c
    rm -rf test_chisel.sh
    if [ ${ARG_REMOVE_RESULT} = "true" ]; then
        rm -rf scores
        rm -rf chisel_out
        rm -rf score*repo
    fi
fi

rm -rf output
rm -rf output.origin
rm -rf testing.output
rm -rf testing.output.origin
rm -rf input
rm -rf testing.input
rm -rf tmp/*
rm -rf test_check*repo
