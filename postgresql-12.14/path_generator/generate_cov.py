#!/usr/bin/env python3

import os,time,shutil, os.path
from sys import argv,version
from multiprocessing import Pool, Manager
import subprocess
from progress.bar import Bar

LLVMPATH="/usr/local/llvm-project/build/bin/"
PROGRAM,COV,DOMGAD_DIR=argv[1:]
CURRDIR=os.getcwd()
SRCs=[src[:-1] for src in open("programlist").readlines()]
merge_sh=f"{CURRDIR}/path_generator/merge_bin_gcov_files.sh"
compile=f"{CURRDIR}/compile.sh"
INDIR=f"{CURRDIR}/testscript/kn"
INDIR_CP=f"{CURRDIR}/"
OUTDIR=f"{CURRDIR}/output.origin/kn"
TMP=f"{CURRDIR}/tmp"
LINEPRINTERBIN=f"timeout -s 9 60s {DOMGAD_DIR}/build/bin/instrumenter -g statement /dev/null --build -- clang -c -I{CURRDIR}/src/common -I{CURRDIR}/src/include"

print(f"-generating {PROGRAM}.c.cov.origin.c")

# subprocess.run(["rm","-rf","lines","programlist"])
# subprocess.run(["touch","lines","programlist"])
subprocess.run(["rm","-rf","lines"])
subprocess.run(["touch","lines"])

#clean output folder #why wouldn't put output folder to 
subprocess.run(["rm","-rf",OUTDIR])
subprocess.run(["mkdir","-p",OUTDIR,"-m","777"])

#use $CURRDIR/tmp for execution
subprocess.run(["rm","-rf",TMP])
subprocess.run(["mkdir","-p",TMP,"-m","777"])


# print("make clean")
# os.system("make clean > /dev/null")


print(f"export COPT='-w'")
os.system(f"export COPT='-w'")
print(f'--CC=clang CFLAGS="-fprofile-instr-generate -fcoverage-mapping" ./configure --prefix={CURRDIR}/pgsql --with-perl --with-tcl --with-python > /dev/null && make > /dev/null && make install > /dev/null')
os.system(f'CC=clang CFLAGS="-fprofile-instr-generate -fcoverage-mapping" ./configure --prefix=$(pwd)/pgsql --with-perl --with-tcl --with-python  > /dev/null')
os.system(f"su postgres -c 'make>/dev/null'")
os.system(f"rm -rf pgsql/*")
os.system(f'su postgres -c "make install>/dev/null"')
os.system(f'su postgres -c "mkdir pgsql/data"')

# programs = os.listdir("pgsql/bin")
programs=["psql"]
print(f"--programs: {programs}")

os.chdir(TMP)
for dir in ("lcov","real.gcov","bin.gcov"):
    print(f"--mkdir -p -m 777 {TMP}/{dir}")
    os.system(f"mkdir -p -m 777 {TMP}/{dir}")

bar = Bar('Processing',max=len(os.listdir(INDIR)))


def DoTestcase(args):
    testcase=args
    bar.next()
    os.system("mkdir -m 777 %s/tmp/%s"%(CURRDIR,testcase))
    os.chdir("%s/tmp/%s"%(CURRDIR,testcase))

    current_env = os.environ.copy()
    current_env["LLVM_PROFILE_FILE"]=f"{TMP}/{testcase}/{testcase}-%p%m.profraw"
    print(' '.join([f"{CURRDIR}/testscript/kn/{testcase}",f"{PROGRAM}",OUTDIR,"1",INDIR_CP]), f'LLVM_PROFILE_FILE={current_env["LLVM_PROFILE_FILE"]}')
    subprocess.run([f"{CURRDIR}/testscript/kn/{testcase}",f"{PROGRAM}",OUTDIR,"1",INDIR_CP], env=current_env)

    print(f"--llvm-profdata merge -o {TMP}/{testcase}/{testcase}.profdata {TMP}/{testcase}/*.profraw")
    os.system(f"{LLVMPATH}/llvm-profdata merge -o {TMP}/{testcase}/{testcase}.profdata {TMP}/{testcase}/*.profraw")

    pname=os.path.basename(PROGRAM)
    print(f"--project name: {pname}")

#    print(f"--llvm-cov export -format=lcov {CURRDIR}/src/bin/psql/psql -instr-profile={TMP}/{testcase}/{testcase}.profdata "+ f" > {TMP}/{testcase}/{pname}.lcov")
#    os.system(f"{LLVMPATH}/llvm-cov export -format=lcov {CURRDIR}/src/bin/psql/psql -instr-profile={TMP}/{testcase}/{testcase}.profdata "+ f" > {TMP}/{testcase}/{pname}.lcov")
    print(f"--llvm-cov export -format=lcov {CURRDIR}/src/backend/postgres -instr-profile={TMP}/{testcase}/{testcase}.profdata "+ f" > {TMP}/{testcase}/{pname}.lcov")
    os.system(f"{LLVMPATH}/llvm-cov export -format=lcov {CURRDIR}/src/backend/postgres -instr-profile={TMP}/{testcase}/{testcase}.profdata "+ f" > {TMP}/{testcase}/{pname}.lcov")

    print(f"--{CURRDIR}/path_generator/delete.py {TMP}/{testcase}/{pname}.lcov {TMP}/{testcase}/{pname}.lcov.reduced && rm {TMP}/{testcase}/{pname}.lcov")
    os.system(f"{CURRDIR}/path_generator/delete.py {TMP}/{testcase}/{pname}.lcov {TMP}/{testcase}/{pname}.lcov.reduced")
    os.system(f"rm {TMP}/{testcase}/{pname}.lcov")

    print(f"--{COV}/bin/lcov2gcov {TMP}/{testcase}/{pname}.lcov.reduced > {TMP}/{testcase}/{pname}.real.gcov")
    os.system(f"{COV}/bin/lcov2gcov {TMP}/{testcase}/{pname}.lcov.reduced > {TMP}/{testcase}/{pname}.real.gcov")

    print(f"--{COV}/bin/gcovanalyzer {TMP}/{testcase}/{pname}.real.gcov getbcov > {TMP}/{testcase}/{pname}.bin.gcov")
    os.system(f"{COV}/bin/gcovanalyzer {TMP}/{testcase}/{pname}.real.gcov getbcov > {TMP}/{testcase}/{pname}.bin.gcov")


    print(f"--sed -i '/^file:/a function:1,0,placeholder' {TMP}/{testcase}/{pname}.bin.gcov")
    os.system(f"sed -i '/^file:/a function:1,0,placeholder' {TMP}/{testcase}/{pname}.bin.gcov")

    print(f"--mv {TMP}/{testcase}/{pname}.lcov.reduced {TMP}/lcov/{testcase}.lcov")
    os.system(f"mv {TMP}/{testcase}/{pname}.lcov.reduced {TMP}/lcov/{testcase}.lcov")
    print(f"--mv {TMP}/{testcase}/{pname}.real.gcov {TMP}/real.gcov/{testcase}.real.gcov")
    os.system(f"mv {TMP}/{testcase}/{pname}.real.gcov {TMP}/real.gcov/{testcase}.real.gcov")
    print(f"--mv {TMP}/{testcase}/{pname}.bin.gcov {TMP}/bin.gcov/{testcase}.bin.gcov")
    os.system(f"mv {TMP}/{testcase}/{pname}.bin.gcov {TMP}/bin.gcov/{testcase}.bin.gcov")

    os.chdir(TMP)
    # os.chmod("%s/tmp/%s"%(CURRDIR,testcase),755)
    #os.system("rm -rf %s/tmp/%s"%(CURRDIR,testcase))

#with Pool(1) as pool:
#    pool.map(DoTestcase, os.listdir(INDIR))
#os.system(f"cp -r {CURRDIR}/tmp {CURRDIR}/tmp_new")
#bar.finish()

os.system(f"cp -r {CURRDIR}/tmp_400_pg/* {CURRDIR}/tmp")
os.system(f"{CURRDIR}/cleanup")

# Note: you can accelerate path_generate process if run more than one time, by copying {CURRDIR}/tmp_new to {CURRDIR}/tmp, as example below shows: 
'''
    #with Pool(1) as pool:
    #    pool.map(DoTestcase, os.listdir(INDIR))
    #os.system(f"cp -r {CURRDIR}/tmp {CURRDIR}/tmp_new")
    #bar.finish()

    os.system(f"cp -r {CURRDIR}/tmp_new/* {CURRDIR}/tmp")
'''

print([merge_sh,COV,TMP])
subprocess.run([merge_sh,COV,TMP])

print(f"{CURRDIR}/path_generator/bin_gcov_spliter.py merged.bin.gcov")
os.system(f"{CURRDIR}/path_generator/bin_gcov_spliter.py merged.bin.gcov")

cov_files = []
'''
with open("merged.bin.gcov") as merged:
    for fn in merged.readlines():
        if(fn.startswith("file:") and fn.endswith(".c\n")):
            cov_files.append(fn[5:-1])
'''
with open(f"{CURRDIR}/programlist") as plst:
    for fn in plst.readlines():
        cov_files.append(fn[:-1])
        # print(cov_files)

#get cov.origin.c
for cf in cov_files:
    coved_cf = cf.replace(".c",".c.cov.origin.c")
    origin_cf=cf.replace(".c",".c.origin.c")
    print(coved_cf)
    dirname=os.path.dirname(f"/tmp/{cf}")
    print(f"{LINEPRINTERBIN} -I{dirname} /tmp/{cf} > {cf}.line.txt")
    os.system(f"{LINEPRINTERBIN} -I{dirname} /tmp/{cf} > {cf}.line.txt")

    # os.system(f"cp {cf} {origin_cf}")
    

    print(' '.join([f"{COV}/bin/gcovbasedcoderemover",f"/tmp/{cf}", f"{cf}.line.txt",f"{cf}.bin.gcov","false",">",coved_cf]))
    os.system(' '.join(["timeout","-s","9","10s",f"{COV}/bin/gcovbasedcoderemover",f"/tmp/{cf}", f"{cf}.line.txt",f"{cf}.bin.gcov","false",">",coved_cf]))
    
    # os.system("cp "+coved_cf+" "+TMP+"/"+os.path.basename(coved_cf))
    print(f"cp {coved_cf} {cf}")
    os.system(f"cp {coved_cf} {cf}")
    os.system(f"{LINEPRINTERBIN} -I{dirname} /tmp/{cf} > {cf}.line.txt")
    print(f"cp {coved_cf} {origin_cf}")
    os.system(f"cp {coved_cf} {origin_cf}")


    os.system(f"echo '{cf}:{cf}.line.txt' >> {CURRDIR}/lines")
    # os.system(f"echo '{cf}' >> {CURRDIR}/programlist")


#get cov_info.txt
#print("extract info")
#shutil.copyfile(f"{CURRDIR}/extract_info.c",f"{TMP}/extract_info.c")
#os.system(" ".join(["g++","extract_info.c","-o","extract_info"]))
#os.system(" ".join(["./extract_info",">",f"{CURRDIR}/Cov_info.json"]))
# os.system(" ".join(["sort","-n",f"{CURRDIR}/Cov_info.txt","-o",f"{CURRDIR}/Cov_info.txt"]))
