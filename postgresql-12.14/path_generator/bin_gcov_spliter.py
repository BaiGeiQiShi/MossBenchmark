#!/usr/bin/env python3
import os,sys
gcovfile = sys.argv[1]

with open(gcovfile) as gcf: 
    lines = gcf.readlines()
    start=0;end=0
    while(start<len(lines)):
        if(lines[start].startswith("file:") and lines[start].endswith(".c\n")):
            end=start+1
            partial_name = lines[start][5:-1]+".bin.gcov" #"file:/foo/bar\n" => "/foo/bar.bin.gcov"
            while(end != len(lines) and not lines[end].startswith("file:")):
                end+=1
            with open(partial_name,"w") as p:
                p.writelines(lines[start:end])
            start=end
        elif(lines[start].startswith("file:")):# but is not a .c file
            start+=1; end+=1
        else:#some errs with this file. Just ignore it, maybe it's because of newlines
            if(len(lines[start])<4):
                exit()
            else:
                start+=1; end+=1;
