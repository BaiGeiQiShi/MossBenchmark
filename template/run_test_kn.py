#!/usr/bin/env python3
  
import os
from sys import argv,version
from multiprocessing import Pool, Manager
import subprocess
import time
CURRDIR=subprocess.getoutput("pwd")
BIN,OUTDIR,TIMEOUT,INDIR,GetRunTest=argv[1:]
GetRunTest=True if GetRunTest=="True" else False
#lock = Manager().Lock()

#clean
try:
    os.system("rm -rf %s"%OUTDIR)
finally:
    os.system(f"mkdir -p {OUTDIR}")

#use a tmp directory for execution
try:
    os.system("rm -rf %s/tmp"%CURRDIR)
finally:
    os.mkdir("%s/tmp"%CURRDIR)

os.chdir("%s/tmp"%CURRDIR)

UsedTime = Manager().dict()
#print("init",id(UsedTime))
#run_test_case
def DoTestcase(args):
    testcase, UsedTime, lock=args
    os.mkdir("%s/tmp/%s"%(CURRDIR,testcase))
    os.chdir("%s/tmp/%s"%(CURRDIR,testcase))
    t1 = time.perf_counter()
    subprocess.run(["/bin/bash",f"{CURRDIR}/testscript/kn/{testcase}",BIN,OUTDIR,TIMEOUT,INDIR])
    t = time.perf_counter()-t1
    os.chmod("%s/tmp/%s"%(CURRDIR,testcase),755)
    os.system("rm -rf %s/tmp/%s"%(CURRDIR,testcase))
    #print(testcase)
    
    if(GetRunTest):
        lock.acquire()
        try: 
            UsedTime[testcase]=t
        finally:
            lock.release()

with Pool(1) as pool:
    lock = Manager().Lock()
    pool.map(DoTestcase, ((t, UsedTime,lock) for t in os.listdir("%s/testscript/kn"%CURRDIR)))

if(GetRunTest):
    times = list(UsedTime.values())
    with open("%s/time_info.txt"%CURRDIR,"w") as info:
#        print("avg: %f\tmax: %f\tmin: %f"%(sum(times)/len(times),max(times),min(times))) 
        print("avg: %f\tmax: %f\tmin: %f"%(sum(times)/len(times),max(times),min(times)), file = info) 
        for kv in sorted(UsedTime.items()):
            print(f"{kv[0]}:{kv[1]}",file = info)

