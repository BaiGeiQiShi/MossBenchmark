#!/bin/bash
  
COV=$1
bin_gcov_path=$2
GCOV_MERGER_BIN=${COV}/bin/gcovbasedcoveragemerger

$GCOV_MERGER_BIN binary $bin_gcov_path/bin.gcov > merged.bin.gcov
