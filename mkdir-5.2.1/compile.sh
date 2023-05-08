#!/bin/bash

src=$1
bin=$2
ops=$3

clang ${ops} -w -o ${bin} ${src}
