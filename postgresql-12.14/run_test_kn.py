#!/usr/bin/env python3
  
import os
from sys import argv,version
from multiprocessing import Pool, Manager
import subprocess
import time
CURRDIR=subprocess.getoutput("pwd")
BIN,OUTDIR,TIMEOUT,INDIR=argv[1:]
GetRunTest=True
#lock = Manager().Lock()

#clean
try:
    os.system("rm -rf %s"%OUTDIR)
finally:
    os.system(f"mkdir -p {OUTDIR}")
    os.chmod(f"{OUTDIR}",511)

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
    os.system('ps -ef | awk \'$1 == "postgres" {print $2}\' | xargs -n 1 kill -s 9')

    os.mkdir("%s/tmp/%s"%(CURRDIR,testcase), mode=0o777)
    os.chmod("%s/tmp/%s"%(CURRDIR,testcase),511)
    os.chdir("%s/tmp/%s"%(CURRDIR,testcase))
    t1 = time.perf_counter()
    try:
        subprocess.run(["/bin/bash",f"{CURRDIR}/testscript/kn/{testcase}",BIN,OUTDIR,TIMEOUT,INDIR], timeout=eval(TIMEOUT))
    except subprocess.TimeoutExpired:
        test_number=testcase[5:]
        os.system(f"echo TIMEOUT >> {OUTDIR}/o{test_number}")
        pass
    t = time.perf_counter()-t1
    #os.chmod("%s/tmp/%s"%(CURRDIR,testcase),511)
    #os.system("rm -rf %s/tmp/%s"%(CURRDIR,testcase))
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

