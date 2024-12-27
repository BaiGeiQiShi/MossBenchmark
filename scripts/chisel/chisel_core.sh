#!/bin/bash

#####
# This script shold be executed in Chisel's working dir.
#####

PROG_NAME=$1
INPUT_SET=$2
SANITIZER="-O3 -w"
debdce=/usr/local/Moss/debdce/build/bin/debdce
TIMEOUT=1

if [ -z ${PROG_NAME} ] || [ -z ${INPUT_SET} ]; then
	echo "Missing arguments."
	exit 1
fi

CURR_DIR=$(pwd)
SRC=$CURR_DIR/$PROG_NAME.c
SRC_ORIGIN=$CURR_DIR/$PROG_NAME.c.real.origin.c
BIN=$CURR_DIR/$PROG_NAME
OUTDIR=$CURR_DIR/output/$INPUT_SET
OUTDIR_ORIGIN=$CURR_DIR/output.origin/$INPUT_SET
INDIR=$CURR_DIR/input
INDIR_ORIGIN=$CURR_DIR/input.origin
TEST_SET=$CURR_DIR/testscript/$INPUT_SET

### Generate Original Outputs
if [ ! -d ${OUTDIR_ORIGIN} ]; then
	mkdir -p ${OUTDIR_ORIGIN}
	./compile.sh ${SRC_ORIGIN} ${BIN} ${SANITIZER} || exit 1

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
	cp -r ${TEST_SET}/* ${INDIR}

    #
    chattr +i ${CURR_DIR}/*
    chattr -i ${CURR_DIR}/tmp ${OUTDIR_ORIGIN} ${OUTDIR}
    #
	cd ${CURR_DIR}/tmp
	for testfile in ${TEST_SET}/*; do
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

		if [ ! -z ${BIN} ]; then
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
	rm $BIN
	cd ${CURR_DIR}
fi

### Generate Reduced Outputs
if [ -f ${BIN} ]; then
	rm ${BIN}
fi

if [ ! -d ${OUTDIR} ]; then
	mkdir -p ${OUTDIR}
else 
	rm -rf ${OUTDIR}/*
fi

inputfname=$(basename $SRC)
if [ -d debdcetmp ]; then
	rm -rf debdcetmp/*
else
        mkdir debdcetmp
fi
cp $SRC debdcetmp/$inputfname
cd debdcetmp
$debdce debdcetest.sh $inputfname &> /dev/null

cd ..
mv debdcetmp/$inputfname.dce.c ${SRC}.tmp.c
rm -rf debdcetmp

./compile.sh ${SRC}.tmp.c ${BIN} ${SANITIZER} || exit 1
rm ${SRC}.tmp.c

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
cp -r ${TEST_SET}/* ${INDIR}

exit_value=0
#
chattr +i ${CURR_DIR}/*
chattr -i ${CURR_DIR}/tmp $OUTDIR $OUTDIR_ORIGIN
#
cd ${CURR_DIR}/tmp
for testfile in ${TEST_SET}/*; do
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
	
	if [ ! -z ${BIN} ]; then
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
			exit_value=1
			break
		fi
	elif [ ! -f ${OUTDIR}/o${testid} ] && [ ! -f ${OUTDIR_ORIGIN}/o${testid} ]; then
		continue
	else
		exit_value=1
		break
	fi
done
#
chattr -i ${CURR_DIR}/*
#

exit ${exit_value}
