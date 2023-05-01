#!/bin/bash

percent=$1

if [ ! -d p${percent}train ]; then
    mkdir p${percent}train
else
    rm p${percent}train/*
fi

if [ ! -d p${percent}test ]; then
    mkdir p${percent}test
else
    rm p${percent}test/*
fi


train_f=p${percent}train.txt
test_f=p${percent}test.txt
>${train_f}
>${test_f}

total=`cat shuffled_tests.txt | wc -l`
echo ${total}

train_num_tmp=`echo "scale=0;${total}*${percent}" | bc -l`
train_num=`echo ${train_num_tmp} | perl -nl -MPOSIX -e 'print floor($_);'`
echo ${train_num}


input=shuffled_tests.txt
counter=0
while IFS= read -r line
do
    if [ ${counter} -lt ${train_num} ]; then
	echo ${line} >> p${percent}train.txt
    else
	echo ${line} >> p${percent}test.txt
    fi
    counter=$((counter + 1))
done < "$input"


~/debaug/bin/bashtestgen p${percent}train.txt p${percent}train
~/debaug/bin/bashtestgen p${percent}test.txt p${percent}test
