#!/usr/bin/env python3
import os
import sys

OPTION=""

try:
    OPTION=" ".join(sys.argv[1:])
except Exception as e:
    print(f"Error when parsing argv:{sys.argv[1:]}")
    exit()

capture_path="debop-out/capture.txt"
if("-t " in OPTION):
    capture_path = OPTION[OPTION.index("-t ")+3:].split()[0]+"/capture.txt"

programlist = set()
with open(f"{capture_path}") as cp:
    for cmd in cp:
        file_name=""
        for op in cmd[:-1].split(","): #trim '\n' in the end of each line 
            if(op.endswith(".c") or op.endswith(".cpp")):
                file_name=op

                programlist.add(file_name)
                break

with open("programlist","w") as srcs:
    print(*programlist, file=srcs, sep="\n")

