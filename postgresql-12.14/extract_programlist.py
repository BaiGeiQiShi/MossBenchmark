#!/usr/bin/env python3.8
import os,re
makelogfile="./makelog"

progs=[]
c_file_pattern = re.compile(r'\b\w+\.c\b')

def starts_with_regex(string):
    return re.match(r'make\[[1-7]\]: Entering directory ', string) is not None

with open(makelogfile, 'r') as file:
    path=""
    for line in file:
        if(starts_with_regex(line[:-1])):
            path=line[29:-2]
            #print(path)
        elif(line.startswith("clang")):
            match = c_file_pattern.search(line)
            if match:
                print(path+"/"+match.group())
