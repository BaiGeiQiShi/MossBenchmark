#!/usr/bin/env python3

import os,time,shutil
from sys import argv,version
from multiprocessing import Pool, Manager
import subprocess
from progress.bar import Bar

PROGRAM,COV=argv[1:]
CURRDIR=os.getcwd()
merge_sh=f"{CURRDIR}/path_generator/merge_bin_gcov_files.sh"
getbin=f"{CURRDIR}/path_generator/get_bin_gcov_file.sh"
compile=f"{CURRDIR}/compile.sh"
INDIR=f"{CURRDIR}/testscript/kn"
INDIR_CP=f"{CURRDIR}/inputfile"
OUTDIR=f"{CURRDIR}/output.origin/kn"
TMP=f"{CURRDIR}/tmp"
print(f"generating {PROGRAM}.c.cov.origin.c")
#clean input folder
subprocess.run(["rm","-rf",INDIR_CP])
subprocess.run(["mkdir","-p",INDIR_CP])

#clean output folder #why wouldn't put output folder to 
subprocess.run(["rm","-rf",OUTDIR])
subprocess.run(["mkdir","-p",OUTDIR])

#use $CURRDIR/tmp for execution
subprocess.run(["rm","-rf",TMP])
subprocess.run(["mkdir","-p",TMP])

os.chdir(TMP)
for dir in ("lcov","real.gcov","bin.gcov"):
    os.mkdir(f"{TMP}/{dir}")

#region copy inputs
os.system(f"cp {CURRDIR}/path_generator/line.txt {TMP}/line.txt")
os.system(f"cp {CURRDIR}/{PROGRAM}.c.real.origin.c {TMP}/{PROGRAM}.c")
os.system(f"cp {CURRDIR}/{PROGRAM}.c.real.origin.c {TMP}/{PROGRAM}.c.real.origin.c")
os.system(f"cp -r {CURRDIR}/input.origin/* {INDIR_CP}")
#endregion
os.system(f'{CURRDIR}/compile.sh {PROGRAM}.c.real.origin.c {PROGRAM} "-fprofile-instr-generate -fcoverage-mapping"')


def DoTestcase(args):
    testcase=args
    print(testcase)
    os.mkdir("%s/tmp/%s"%(CURRDIR,testcase))
    os.chdir("%s/tmp/%s"%(CURRDIR,testcase))
    subprocess.run([f"{CURRDIR}/testscript/kn/{testcase}",f"{TMP}/{PROGRAM}",OUTDIR,"4",INDIR_CP,"%s/tmp/%s"%(CURRDIR,testcase)])
    if(not os.path.exists(f"{os.getcwd()}/default.profraw")):
        raise RuntimeError(testcase)
    os.system(" ".join([getbin, f"{TMP}/{PROGRAM}",COV,os.getcwd()]))
    os.system(f"mv {PROGRAM}.lcov {TMP}/lcov/{testcase}.lcov")
    os.system(f"mv {PROGRAM}.real.gcov {TMP}/real.gcov/{testcase}.real.gcov")
    os.system(f"mv {PROGRAM}.bin.gcov {TMP}/bin.gcov/{testcase}.bin.gcov")
    os.chdir(TMP)
    os.chmod("%s/tmp/%s"%(CURRDIR,testcase),755)
    os.system("rm -rf %s/tmp/%s"%(CURRDIR,testcase))

with Pool(1) as pool:
    pool.map(DoTestcase, os.listdir(INDIR))

#get cov.origin.c
subprocess.run([merge_sh,COV,TMP])
os.system(' '.join([f"{COV}/bin/gcovbasedcoderemover",f"{PROGRAM}.c", "line.txt","merged.bin.gcov","false",">",f"{PROGRAM}.c.cov.origin.c"]))
'''
Don't change cov.origin.c in CURRDIR, for $cov cannot generate an acceptable coverage file. We add back some codes in vim-5.8.c.cov.origin.c about fork and don't want to override it.
'''
#shutil.copyfile(f"{PROGRAM}.c.cov.origin.c",f"{CURRDIR}/{PROGRAM}.c.cov.origin.c")

#get cov_info.txt
shutil.copyfile(f"{CURRDIR}/extract_info.c",f"{TMP}/extract_info.c")
os.system(" ".join(["g++","extract_info.c","-o","extract_info"]))
os.system(" ".join(["./extract_info",">",f"{CURRDIR}/Cov_info.txt"]))
os.system(" ".join(["sort","-n",f"{CURRDIR}/Cov_info.txt","-o",f"{CURRDIR}/Cov_info.txt"]))
os.system(f"{CURRDIR}/cleanup")


