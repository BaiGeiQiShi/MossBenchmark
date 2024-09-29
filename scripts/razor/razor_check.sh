#!/bin/bash

#####
# This script shold be executed in Razor's working dir.
#####

PROG_NAME=$1
INPUT_SET=$2
SANITIZER=$3

TIMEOUT=10

if [ -z ${PROG_NAME} ] || [ -z ${INPUT_SET} ]; then
	echo "Missing arguments."
	exit 1
fi
progabbr=`echo ${PROG_NAME} | cut -d'-' -f1`
CURR_DIR=$(pwd)
BIN_COV=$CURR_DIR/razor_code/reduced/${INPUT_SET}_cov/$PROG_NAME
BIN_COV_L1=$CURR_DIR/razor_code/reduced/${INPUT_SET}_covl1/$PROG_NAME
BIN_COV_L2=$CURR_DIR/razor_code/reduced/${INPUT_SET}_covl2/$PROG_NAME
BIN_COV_L3=$CURR_DIR/razor_code/reduced/${INPUT_SET}_covl3/$PROG_NAME
BIN_COV_L4=$CURR_DIR/razor_code/reduced/${INPUT_SET}_covl4/$PROG_NAME
BIN_ORIGIN=$CURR_DIR/razor_code/origin/$PROG_NAME
BIN=$CURR_DIR/${progabbr}.orig
OUTDIR=$CURR_DIR/testing.output
OUTDIR_ORIGIN=$CURR_DIR/testing.output.origin
INDIR=$CURR_DIR/testing.input
INDIR_ORIGIN=$CURR_DIR/input.origin

REPORT_FILE=$CURR_DIR/test_check_${INPUT_SET}.repo

### Generate Original Outputs
if [ ! -d ${OUTDIR_ORIGIN} ]; then
	mkdir -p ${OUTDIR_ORIGIN}

	if [ -f ${BIN} ]; then
		rm ${BIN}
	fi
	cp ${BIN_ORIGIN} ${BIN}
	chmod 755 ${BIN}

	if [ ! -d ${CURR_DIR}/tmp ]; then
		mkdir ${CURR_DIR}/tmp
	else
		chmod -R 755 ${CURR_DIR}/tmp
		rm -rf ${CURR_DIR}/tmp/*
	fi

	if [ -d ${INDIR} ]; then
		rm -rf ${INDIR}/*
	else
		mkdir ${INDIR}
	fi
	if [ -d ${INDIR_ORIGIN} ]; then
		cp -r ${INDIR_ORIGIN}/* ${INDIR}
		# 
	fi

	if [ -d ${CURR_DIR}/testscript/${INPUT_SET}.testing ]; then
		rm -rf ${CURR_DIR}/testscript/${INPUT_SET}.testing
	fi
	cp -r ${CURR_DIR}/testscript/kn ${CURR_DIR}/testscript/${INPUT_SET}.testing
	cp -r ${CURR_DIR}/testscript/kn/* ${INDIR}/
	
	#
	chattr +i ${CURR_DIR}/*
	chattr -i ${CURR_DIR}/tmp ${OUTDIR_ORIGIN} ${OUTDIR}
	#
	cd ${CURR_DIR}/tmp
	for testfile in ${CURR_DIR}/testscript/${INPUT_SET}.testing/*; do
		tmpdir=${CURR_DIR}/tmp/$(basename ${testfile})	
		if [ ! -d $tmpdir ]; then
			mkdir $tmpdir
		else
			rm -rf $tmpdir/*
		fi
		cd $tmpdir
		${testfile} ${BIN} ${OUTDIR_ORIGIN} $TIMEOUT $INDIR

		cd ${CURR_DIR}/tmp
	  	chmod 755 -R $tmpdir
	  	rm -rf $tmpdir

		if [ -z ${BIN} ]; then
			procs=$(ps aux | awk -v var="${BIN}" '($11 == var)' | sed 's/\s\s*/ /g' | cut -d' ' -f2)
			if [ ! -z "${procs}" ]; then
				echo ${procs} >mykills.sh
				sed -e 's|^|kill -9 |g' -i mykills.sh
				chmod 700 mykills.sh
				./mykills.sh
				rm mykills.sh
			fi

		fi
	done
	#
	chattr -i ${CURR_DIR}/*
	#
	rm -rf ${INDIR}
	rm -rf ${CURR_DIR}/testscript/${INPUT_SET}.testing
	rm -rf ${CURR_DIR}/logs/cbr_indcall.*
	cd ${CURR_DIR}
fi

>${REPORT_FILE}
### Generate Reduced Outputs
function reduced_test(){
	testing=$1
	testall=$2

	>${REPORT_FILE}.tmp
	
	if [ -f ${BIN} ]; then
		rm ${BIN}
	fi

	case ${testing} in
		0)
			cp ${BIN_COV} ${BIN}
			TESTING_NAME="debloat"
			;;
		1)
			cp ${BIN_COV_L1} ${BIN}
			TESTING_NAME="debloat_extl1"
			;;
		2)
			cp ${BIN_COV_L2} ${BIN}
			TESTING_NAME="debloat_extl2"
			;;
		3)
			cp ${BIN_COV_L3} ${BIN}
			TESTING_NAME="debloat_extl3"
			;;
		4)
			cp ${BIN_COV_L4} ${BIN}
			TESTING_NAME="debloat_extl4"
			;;
	esac
	chmod 755 ${BIN}

	echo "Now testing: ${TESTING_NAME} ${testall}" | tee -a ${REPORT_FILE}

	if [ ! -d ${OUTDIR} ]; then
		mkdir -p ${OUTDIR}
	else 
		rm -rf ${OUTDIR}/*
	fi

	if [ ! -d ${CURR_DIR}/tmp ]; then
		mkdir ${CURR_DIR}/tmp
	else
		chmod -R 755 ${CURR_DIR}/tmp
		rm -rf ${CURR_DIR}/tmp/*
	fi

	if [ -d ${INDIR} ]; then
		rm -rf ${INDIR}/*
	else
		mkdir ${INDIR}
	fi
	if [ -d ${INDIR_ORIGIN} ]; then
		cp -r ${INDIR_ORIGIN}/* ${INDIR}
	fi

	if [ -d ${CURR_DIR}/testscript/${INPUT_SET}.testing ]; then
		rm -rf ${CURR_DIR}/testscript/${INPUT_SET}.testing
	fi
	if [ -z ${testall} ]; then
		cp -r ${CURR_DIR}/testscript/${INPUT_SET} ${CURR_DIR}/testscript/${INPUT_SET}.testing
		cp -r ${CURR_DIR}/testscript/${INPUT_SET}/* ${INDIR}
	else
		cp -r ${CURR_DIR}/testscript/kn ${CURR_DIR}/testscript/${INPUT_SET}.testing
		cp -r ${CURR_DIR}/testscript/kn/* ${INDIR}
	fi
	#
	chattr +i ${CURR_DIR}/*
	chattr -i ${CURR_DIR}/tmp ${OUTDIR_ORIGIN} ${OUTDIR} ${REPORT_FILE}.tmp
	#
	cd ${CURR_DIR}/tmp
	for testfile in ${CURR_DIR}/testscript/${INPUT_SET}.testing/*; do
		tmpdir=${CURR_DIR}/tmp/$(basename ${testfile})	
		if [ ! -d $tmpdir ]; then
			mkdir $tmpdir
		else
			rm -rf $tmpdir/*
		fi
		cd $tmpdir
		${testfile} ${BIN} ${OUTDIR} ${TIMEOUT} ${INDIR}

		cd ${CURR_DIR}/tmp
	  	chmod 755 -R $tmpdir
	  	rm -rf $tmpdir

		if [ -z ${BIN} ]; then
			procs=$(ps aux | awk -v var=${BIN} '($11 == var)' | sed 's/\s\s*/ /g' | cut -d' ' -f2)
			if [ ! -z "${procs}" ]; then
				echo ${procs} >mykills.sh
				sed -e 's|^|kill -9 |g' -i mykills.sh
				chmod 700 mykills.sh
				./mykills.sh
				rm mykills.sh
			fi
		fi

		#Compare Outputs
		testid=$(basename ${testfile} | sed 's/^test_//g')
		if [ -f ${OUTDIR}/o${testid} ] && [ -f ${OUTDIR_ORIGIN}/o${testid} ]; then
			if ! diff -q ${OUTDIR}/o${testid} ${OUTDIR_ORIGIN}/o${testid} &> /dev/null; then
				echo "fail-test_${testid}" >> ${REPORT_FILE}.tmp
			else
				echo "pass-test_${testid}" >> ${REPORT_FILE}.tmp
			fi
		elif [ ! -f ${OUTDIR}/o${testid} ] && [ ! -f ${OUTDIR_ORIGIN}/o${testid} ]; then
			echo "pass-test_${testid}" >> ${REPORT_FILE}.tmp
		else
			echo "fail-test_${testid}" >> ${REPORT_FILE}.tmp
		fi
	done
	#
	chattr -i ${CURR_DIR}/*
	#
	rm -rf ${INDIR}
	rm -rf ${CURR_DIR}/testscript/${INPUT_SET}.testing
	rm -rf ${CURR_DIR}/logs/cbr_indcall.*

	pass_all=$(grep 'pass-' ${REPORT_FILE}.tmp | wc -l)
	fail_all=$(grep 'fail-' ${REPORT_FILE}.tmp | wc -l)

	rm ${REPORT_FILE}.tmp
	
	echo "Pass: ${pass_all}" | tee -a ${REPORT_FILE}
	echo "Fail: ${fail_all}" | tee -a ${REPORT_FILE}
	echo "Pass rate${testall}: $(awk 'BEGIN{printf "%.16f\n",('$pass_all'/('$pass_all'+'$fail_all'))}')" | tee -a ${REPORT_FILE}
}

for i in {0..4}; do
	reduced_test $i
	reduced_test $i "(All)"
done


if [ -f ${BIN} ]; then
	rm ${BIN}
fi

if [ -d ${OUTDIR} ]; then
	rm -rf ${OUTDIR}
fi

if [ -d ${OUTDIR_ORIGIN} ]; then
	rm -rf ${OUTDIR_ORIGIN}
fi

if [ -d ${INDIR} ]; then
	rm -rf ${INDIR}
fi
