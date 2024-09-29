#!/bin/bash

######
# NOTE: This script has to be run within the program's working dir.
######

progname=$1
inputset=$2

if [ -z ${progname} ] || [ -z ${inputset} ]; then
    echo "Missing arguments."
fi

progabbr=`echo ${progname} | cut -d'-' -f1`
CURRDIR=$(pwd)
BIN=$CURRDIR/${progabbr}.orig
RAZOR_DIR=/razor
DRRUN=${RAZOR_DIR}/tracers/dynamorio/bin64/drrun
CLIENT=$CURRDIR/logs/libcbr_indcall.so
TIMEOUT=30


#Prepare logs dir
if [ -d logs ]; then
    rm -fr logs/*
else
    mkdir -p ./logs
fi
cp ${RAZOR_DIR}/tracers/bin/libcbr_indcall.so ./logs/


#Compile
if [ -f $BIN ]; then
    rm $BIN
fi
./compile.sh ${progname}.c $BIN "-g -w -Wl,--build-id"

#Prepare input dir
if [ -d input ]; then
    rm -fr input/*
else
    mkdir input
fi

input_origin_dir=${CURRDIR}/input.origin
if [ -d ${input_origin_dir} ]; then
	cp -r ${input_origin_dir}/* ./input/
fi
cp -r $CURRDIR/testscript/${inputset}/* ./input/

#Prepare output dir
if [ -d output.origin/${inputset} ]; then
    rm -fr output.origin/${inputset}/*
else
    mkdir -p output.origin/${inputset}
fi


#Prepare a tmp dir for execution
if [ ! -d $CURRDIR/tmp ]; then
    mkdir $CURRDIR/tmp
else
    chmod 755 -R $CURRDIR/tmp
    rm -fr $CURRDIR/tmp/*
fi
cd $CURRDIR/tmp


#Run tests
for testf in $CURRDIR/testscript/${inputset}/*; do
	tmpdir=$CURRDIR/tmp/$(basename ${testf})	
	if [ ! -d $tmpdir ]; then
		mkdir $tmpdir
	else
		rm -rf $tmpdir/*
	fi
	cd $tmpdir
	${testf} $BIN ${CURRDIR}/output.origin/${inputset} $TIMEOUT ${CURRDIR}/input

	cd $CURRDIR/tmp
  	chmod 755 -R $tmpdir
  	rm -rf $tmpdir

  	#Kill the non-terminated ones
  	if [ ! -z $BIN ]; then
    	  #Look for commands that start with $BIN ($11 is the start of command)
      	procs=`ps aux | awk -v var="$BIN" '($11 == var)' | sed 's/\s\s*/ /g' | cut -d' ' -f2`
      	if [ ! -z "${procs}" ]; then
          	echo ${procs} >mykills.sh
          	sed -e 's|^|kill -9 |g' -i mykills.sh
            chmod 700 mykills.sh
            ./mykills.sh
            rm mykills.sh
        fi
    fi    
    
done

chmod 755 -R $CURRDIR/tmp
rm -rf $CURRDIR/tmp/*
rm -rf $CURRDIR/input
