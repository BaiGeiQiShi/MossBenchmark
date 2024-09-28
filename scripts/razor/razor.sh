#!/bin/bash

PROG_NAME=$1
INPUT_SET=$2

if [ -z ${PROG_NAME} ] || [ -z ${INPUT_SET} ]; then
    echo "Missing arguments."
    exit 1
fi

PROG_ABBR=$(echo ${PROG_NAME} | cut -d'-' -f1)

DIR_ORIGIN=razor_code/origin
DIR_COV=razor_code/reduced/${INPUT_SET}_cov
DIR_COV_L1=razor_code/reduced/${INPUT_SET}_covl1
DIR_COV_L2=razor_code/reduced/${INPUT_SET}_covl2
DIR_COV_L3=razor_code/reduced/${INPUT_SET}_covl3
DIR_COV_L4=razor_code/reduced/${INPUT_SET}_covl4

if [ ! -d ${DIR_ORIGIN} ]; then
	mkdir -p ${DIR_ORIGIN}
fi
if [ ! -d ${DIR_COV} ]; then
	mkdir -p ${DIR_COV}
fi
if [ ! -d ${DIR_COV_L1} ]; then
	mkdir -p ${DIR_COV_L1}
fi
if [ ! -d ${DIR_COV_L2} ]; then
	mkdir -p ${DIR_COV_L2}
fi
if [ ! -d ${DIR_COV_L3} ]; then
	mkdir -p ${DIR_COV_L3}
fi
if [ ! -d ${DIR_COV_L4} ]; then
	mkdir -p ${DIR_COV_L4}
fi

LOG_FILE="./log/log_razor_${INPUT_SET}_$(date +'%Y-%m-%d_%H:%M:%S').txt"

>${LOG_FILE}

### Training
start_time_s=$(date +%s)
./razor_train.sh ${PROG_NAME} ${INPUT_SET} &>> ${LOG_FILE}
end_time_s=$(date +%s)
time_dur=$[ ${end_time_s} - ${start_time_s} ]
echo "Training time: ${time_dur}s" &>> ${LOG_FILE}

cp ${PROG_ABBR}.orig ${DIR_ORIGIN}/${PROG_NAME}

#Cov
start_time_s=$(date +%s)
python razor.py ${PROG_NAME} debloat &>> ${LOG_FILE}
end_time_s=$(date +%s)
time_dur=$[ ${end_time_s} - ${start_time_s} ]
echo "Debloat time: ${time_dur}s" &>> ${LOG_FILE}

cp ${PROG_ABBR}.orig_temp/${PROG_ABBR}.orig.debloated ${DIR_COV}/${PROG_NAME}

#Extend L1
start_time_s=$(date +%s)
python razor.py ${PROG_NAME} extend_debloat 1 &>> ${LOG_FILE}
end_time_s=$(date +%s)
time_dur=$[ ${end_time_s} - ${start_time_s} ]
echo "Extend debloat level 1 time: ${time_dur}s" &>> ${LOG_FILE}

cp ${PROG_ABBR}.orig_temp/${PROG_ABBR}.orig.debloated ${DIR_COV_L1}/${PROG_NAME}

#Extend L2
start_time_s=$(date +%s)
python razor.py ${PROG_NAME} extend_debloat 2 &>> ${LOG_FILE}
end_time_s=$(date +%s)
time_dur=$[ ${end_time_s} - ${start_time_s} ]
echo "Extend debloat level 2 time: ${time_dur}s" &>> ${LOG_FILE}

cp ${PROG_ABBR}.orig_temp/${PROG_ABBR}.orig.debloated ${DIR_COV_L2}/${PROG_NAME}

#Extend L3
start_time_s=$(date +%s)
python razor.py ${PROG_NAME} extend_debloat 3 &>> ${LOG_FILE}
end_time_s=$(date +%s)
time_dur=$[ ${end_time_s} - ${start_time_s} ]
echo "Extend debloat level 3 time: ${time_dur}s" &>> ${LOG_FILE}

cp ${PROG_ABBR}.orig_temp/${PROG_ABBR}.orig.debloated ${DIR_COV_L3}/${PROG_NAME}

#Extend L4
start_time_s=$(date +%s)
python razor.py ${PROG_NAME} extend_debloat 4 &>> ${LOG_FILE}
end_time_s=$(date +%s)
time_dur=$[ ${end_time_s} - ${start_time_s} ]
echo "Extend debloat level 4 time: ${time_dur}s" &>> ${LOG_FILE}

cp ${PROG_ABBR}.orig_temp/${PROG_ABBR}.orig.debloated ${DIR_COV_L4}/${PROG_NAME}

rm -rf ${PROG_ABBR}.orig
rm -rf ${PROG_ABBR}.orig_temp

