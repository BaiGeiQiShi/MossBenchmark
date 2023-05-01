#!/usr/bin/env python3.8
import subprocess,os
#region ENVSandARGS
METHOD={"DEBOP":0,"BASICBLOCK":1,"COVBLOAT":2,"TMCMC":3,"MOSS":4}
PROGNAME="bash-2.05"
version=str.upper("TMCMC")
debop_samplenum=str(100000)
domgad_samplenum=str(5)
TIMEOUT="5m"
alphas=list(map(str,[0.5,]))
ks=map(str,[50,])
betas=list(map(str,[0.5]))
CURRDIR=os.getcwd()
DEBOP_DIR="/usr/local/Moss/CovBlock_Stmt"
DEBOP_BIN=f"{DEBOP_DIR}/build/bin/reducer"
DOMGAD_DIR="/usr/local/Moss/CovPath"
COV="/usr/local/cov"
LINEPRINTERBIN=f"{DOMGAD_DIR}/build/bin/instrumenter -g statement test.sh"
SEARCHBIN=f"java -cp ':{DOMGAD_DIR}/build/java:{DOMGAD_DIR}/lib/java/*' edu.gatech.cc.domgad.GCovBasedMCMCSearch"
iternum=str(100000000)
realorcov="cov"
filter="nodeclstmt"
#endregion ENVSandARGS
def DEBOP(_rid):
    try:
        best=subprocess.check_output(f"timeout -s 9 {TIMEOUT} {DEBOP_BIN} -M Cov_info.txt -T COVBLOATBEST.c -m {debop_samplenum} -i {iternum} -t debop-out.{_rid} -a {alpha} -e {beta} -k {k} -s ./test.sh {PROGNAME}.c > log/{_rid}.txt",shell=True)
    except subprocess.CalledProcessError as e:
        if(e.returncode==137):pass
        else:raise e
    subprocess.run(["/usr/local/bin/getLog.py",f"{CURRDIR}/log/{_rid}.txt", f"{CURRDIR}/log/stat.{_rid}.txt"])
    with open(f"{CURRDIR}/log/stat.{_rid}.txt") as rid:
        best=rid.readline().split()
        if(best[0]!="-1"):
            os.system(f"cp {CURRDIR}/debop-out.{_rid}/{PROGNAME}.c.sample{best[0]}.c {CURRDIR}/DEBOPBEST.c")
        else:
            os.system(f"cp {CURRDIR}/COVBLOATBEST.c {CURRDIR}/DEBOPBEST.c")

def BASICBLOCK(_rid):
    try:
        best=subprocess.check_output(f"timeout -s 9 {TIMEOUT} {DEBOP_BIN} -B -m {debop_samplenum} -i {iternum} -t debop-out.{_rid} -a {alpha} -e {beta} -k {k} -s ./test.sh {PROGNAME}.c > log/{_rid}.txt",shell=True)
    except subprocess.CalledProcessError as e:
        if(e.returncode==137):pass
        else:raise e
    subprocess.run(["/usr/local/bin/getLog.py",f"{CURRDIR}/log/{_rid}.txt", f"{CURRDIR}/log/stat.{_rid}.txt"])
    with open(f"{CURRDIR}/log/stat.{_rid}.txt") as rid:
        best=rid.readline().split()
        if(best[0]!="-1"):
            os.system(f"cp {CURRDIR}/debop-out.{_rid}/{PROGNAME}.c.sample{best[0]}.c {CURRDIR}/BASICBLOCKBEST.c")
        else:
            os.system(f"cp {CURRDIR}/{PROGNAME}.c.cov.origin.c {CURRDIR}/BASICBLOCKBEST.c")

def COVBLOAT(_rid):
    try:
        subprocess.run(f"timeout -s 9 {TIMEOUT} {DEBOP_BIN} -F ./Cov_info.txt -T TMCMCBEST.c -m {debop_samplenum} -i {iternum} -t debop-out.{_rid} -a {alpha} -e {beta} -k {k} -s ./test.sh {PROGNAME}.c > log/{_rid}.txt",shell=True)
    except subprocess.CalledProcessError as e:
        if(e.returncode==137):pass
        else:raise e
    subprocess.run(["/usr/local/bin/getLog.py",f"{CURRDIR}/log/{_rid}.txt", f"{CURRDIR}/log/stat.{_rid}.txt"])
    with open(f"{CURRDIR}/log/stat.{_rid}.txt") as rid:
        best=rid.readline().split()
        if(best[0]!="-1"):
            os.system(f"cp {CURRDIR}/debop-out.{_rid}/{PROGNAME}.c.sample{best[0]}.c {CURRDIR}/COVBLOATBEST.c")
        else:
            os.system(f"cp {CURRDIR}/TMCMCBEST.c {CURRDIR}/COVBLOATBEST.c")

def TMCMC(alpha,beta,k):
    #make identify_path and quantify_path
    os.system("rm -rf identify_path quantify_path domgad_sample_output")
    os.system("mkdir identify_path")
    for test in os.listdir(f"{CURRDIR}/tmp/bin.gcov"):
        subprocess.run(["cp",f"{CURRDIR}/tmp/bin.gcov/{test}",f"{CURRDIR}/identify_path/{test[5:].split('.')[0]}"])
    os.system(f"cp -r {CURRDIR}/identify_path {CURRDIR}/quantify_path")
    #generate path_count.txt
    with open(f"{CURRDIR}/tmp/path_counted.txt","w+") as pc:
        for pid,ipath in enumerate(sorted(os.listdir(f"{CURRDIR}/identify_path"))):
            print(f"{pid},{ipath},1,{ipath}",file=pc)
    #run domgad stochasticsearch.sh
    quan_num=str(len(os.listdir(f"{CURRDIR}/quantify_path")))
    os.system(f"echo {quan_num} > {CURRDIR}/tmp/quan_num.txt")

    subprocess.run(["cp",f"{CURRDIR}/getsize.sh",f"{CURRDIR}/tmp/getsize.sh"])
    subprocess.run(["cp",f"{CURRDIR}/compile.sh",f"{CURRDIR}/tmp/compile.sh"])
    rid=f"{realorcov}.{filter}.s{domgad_samplenum}.a{alpha}.b{beta}.k{k}.v3"
    os.system(" ".join(["timeout","-s", "9", TIMEOUT, SEARCHBIN,f"{CURRDIR}/tmp/path_counted.txt",f"{CURRDIR}/identify_path",f"{CURRDIR}/tmp/sample_output",domgad_samplenum,f"{CURRDIR}/tmp/{PROGNAME}.c",f"{CURRDIR}/tmp/line.txt",f"{CURRDIR}/tmp",PROGNAME,alpha,beta,k,quan_num,"1",f"{CURRDIR}/BaseInputs.txt"])+f">{CURRDIR}/log/{rid}.txt")
    subprocess.run(["cp","tmp/sample_output",f"{CURRDIR}/domgad_sample_output","-r"])
    subprocess.run(["/usr/local/bin/getLog.py",f"{CURRDIR}/log/{rid}.txt", f"{CURRDIR}/log/stat.{rid}.txt"])
    with open(f"{CURRDIR}/log/stat.{rid}.txt") as rid:
        best=rid.readline().split()
        os.system(f"cp {CURRDIR}/tmp/sample_output/{best[0]}.c {CURRDIR}/TMCMCBEST.c")

#os.system(f"{LINEPRINTERBIN} {CURRDIR}/{PROGNAME}.c.real.origin.c > {CURRDIR}/path_generator/line.txt")
##########################################subprocess.run([f"{CURRDIR}/basegen.sh", PROGNAME, COV])
#subprocess.run([f"{CURRDIR}/path_generator/generate_cov.py", PROGNAME, COV])
#subprocess.run(["cp",f"{CURRDIR}/{PROGNAME}.c.base.origin.c",f"{CURRDIR}/tmp"])

for k in ks:
    for alpha in alphas:
        for beta in betas:
            os.system(f"{LINEPRINTERBIN} {CURRDIR}/{PROGNAME}.c.real.origin.c > {CURRDIR}/path_generator/line.txt")
            #subprocess.run([f"{CURRDIR}/path_generator/generate_cov.py", PROGNAME, COV])
            subprocess.run(["cp",f"{CURRDIR}/{PROGNAME}.c.base.origin.c",f"{CURRDIR}/tmp"])


#            print(beta);os.system("sleep 1")
            #region init envs and do some cleaning
            os.system(f"echo {PROGNAME}.c.origin.c {PROGNAME}.c | xargs -n 1 cp {PROGNAME}.c.{realorcov}.origin.c")
#            subprocess.run(" ".join(["rm","-rf","output.origin","inputfile","*BEST.c"]),shell=True)
            subprocess.run(" ".join(["rm","-rf","output.origin","inputfile","*BEST.c"]),shell=True)
            # subprocess.run(["source","/etc/profile"])
            #endregion init envs and do some cleaning

            if(version=="MOSS"):
                for subversion in ("TMCMC","COVBLOAT","DEBOP"):
                    try:
#                        os.system(f"echo {alpha} {beta} {subversion} >>check.txt")
                        if(subversion=="TMCMC"):
                            eval(subversion)(alpha,beta,k)
                        else:
                            eval(subversion)(f"{realorcov}.{filter}.s{debop_samplenum}.a{alpha}.b{beta}.k{k}.v{METHOD[subversion]}")
                        #subprocess.run(["cp",f"{CURRDIR}/{subversion}BEST.c",f"{CURRDIR}/{PROGNAME}.c.cov.origin.c"])
                        subprocess.run(["cp",f"{CURRDIR}/{PROGNAME}.c.cov.origin.c",f"{CURRDIR}/{PROGNAME}.c"])
                    except IndexError as ie:
                        print(ie) #just pass this iter if it do nothing

            else:
                rid = f"{realorcov}.{filter}.s{debop_samplenum}.a{alpha}.b{beta}.k{k}.v{METHOD[version]}"
                while(True):
                    try:
                        if(version=="DEBOP"):
                            DEBOP(rid)
                        elif(version=="BASICBLOCK"):
                            BASICBLOCK(rid)
                        elif(version=="COVBLOAT"):
                            COVBLOAT(rid)
                        elif(version=="TMCMC"):
                            TMCMC(alpha,beta,k)
                            rid=f"a{alpha}.b{beta}.k{k}.mcmclog"
                        
                        break
                    except subprocess.CalledProcessError as e:#error and need restart
                        print(e)
