#!/usr/bin/env python3.8
import os
CURRDIR=os.getcwd()
with open(f"{CURRDIR}/tmp/path_counted.txt","w+") as pc:
    for pid,ipath in enumerate(sorted(os.listdir(f"{CURRDIR}/identify_path"))):
        print(f"{pid},{ipath},1,{ipath}",file=pc)  
