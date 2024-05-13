#!/bin/bash

src=$1
bin=$2
ops=$3
libglob_path=lib/libglob.a
clang ${ops} -o ${bin} ${src} ${libglo_path} -lutil
