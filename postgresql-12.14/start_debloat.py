#!/usr/bin/env python3.8
import subprocess,os, time, sys
import json
#region ENVSandARGS
METHOD={"DEBOP":0,"BASICBLOCK":1,"COVBLOAT":2,"TMCMC":3,"MOSS":4}
PROGNAME="psql"
version=str.upper("MOSS")
debop_samplenum=str(100000)
domgad_samplenum=str(100000)
TMCMC_TIMEOUT="4h"
TIMEOUT="4h"
#alphas=list(map(str,[0.25,0.5,0.75]))
alphas=[sys.argv[1]]
ks=list(map(str,[50,]))
#betas=list(map(str,[0.25, 0.5, 0.75]))
betas=[sys.argv[2]]
CURRDIR=os.getcwd()
DEBOP_DIR="/usr/local/Moss/multiFiles/CovBlock_Stmt"
DEBOP_BIN=f"{DEBOP_DIR}/build/bin/reducer"
DOMGAD_DIR="/usr/local/Moss/multiFiles/CovPath"
COV="/usr/local/Moss/cov"
LINEPRINTERBIN=f"{DOMGAD_DIR}/build/bin/instrumenter -g statement test.sh"
SEARCHBIN=f"java -Xmx64g  -cp ':{DOMGAD_DIR}/build/java:{DOMGAD_DIR}/lib/java/*:{DOMGAD_DIR}/lib/java/commons-cli-1.5.0/*' moss.covpath.GCovBasedMCMCSearch"
# SEARCHBIN=f"java -cp ':{DOMGAD_DIR}/build/java:{DOMGAD_DIR}/lib/java/*:{DOMGAD_DIR}/lib/java/commons-cli-1.5.0/*' -Djava.util.logging.config.file={DOMGAD_DIR}/src/java/main/logging.properties moss.covpath.GCovBasedMCMCSearch"
iternum=str(100000)
realorcov="cov"
filter="nodeclstmt"
#endregion ENVSandARGS

def BESTTEMP_builder(stage, sampleid):
    '''
    cp programlist(a list-like file) -> XXBEST(a json file), and wrap with corresponding suffix
    '''
    relation={}
    progs=[]
    with open("programlist") as pl:
        for p in pl.readlines():
            p=p[:-1]
            if(stage=="TMCMC"):
                relation[p]=p.replace(".c",f".sample{sampleid}.c")
            else:
                relation[p]=p+f".sample{sampleid}.c"
    with open(str.upper(stage)+"BEST","w") as bf:
        json.dump(relation, bf)

def DEBOP(_rid):
    os.system("rm originscore")
    try:
        subprocess.check_output(f"timeout -s 9 {TIMEOUT} {DEBOP_BIN} -M Cov_info.json -T COVBLOATBEST -m {debop_samplenum} -i {iternum} -t {CURRDIR}/moss-out.{_rid} -a {alpha} -e {beta} -k {k} -P {CURRDIR}/programlist -s ./test.sh --build -- make > log/{_rid}.txt",shell=True)
        os.system(f"sort -k 8 {CURRDIR}/moss-out.{_rid}/Simplifiedlog.txt -nrb | head -n 1 > log/stat.{_rid}.txt && cat {CURRDIR}/moss-out.{_rid}/Simplifiedlog.txt >> log/stat.{_rid}.txt && awk -F'\t' '{{print $2\"\\t\"$3\"\\t\"$4\"\\t\"$5\"\\t\"$6\"\\t\"$7\"\\t\"$8}}' log/stat.{_rid}.txt > log/temp && mv log/temp log/stat.{_rid}.txt")
    except subprocess.CalledProcessError as e:
        if(e.returncode==137):pass
        else:pass

def COVBLOAT(_rid):
    os.system("rm originscore")
    try:
        print(f"timeout -s 9 {TIMEOUT} {DEBOP_BIN} -F ./Cov_info.json -T TMCMCBEST -m {debop_samplenum} -i {iternum} -t {CURRDIR}/moss-out.{_rid} -a {alpha} -e {beta} -k {k} -P {CURRDIR}/programlist -s ./test.sh --build -- make > log/{_rid}.txt")
        subprocess.run(f"timeout -s 9 {TIMEOUT} {DEBOP_BIN} -v -F ./Cov_info.json -T TMCMCBEST -m {debop_samplenum} -i {iternum} -t moss-out.{_rid} -a {alpha} -e {beta} -k {k} -P {CURRDIR}/programlist -s ./test.sh --build -- make > log/{_rid}.txt",shell=True)
    except subprocess.CalledProcessError as e:
        if(e.returncode==137):pass
        else:raise e
    os.system(f"sort -k 8 {CURRDIR}/moss-out.{_rid}/Simplifiedlog.txt -nrb | head -n 1 > log/stat.{_rid}.txt && cat {CURRDIR}/moss-out.{_rid}/Simplifiedlog.txt >> log/stat.{_rid}.txt && awk -F'\t' '{{print $2\"\\t\"$3\"\\t\"$4\"\\t\"$5\"\\t\"$6\"\\t\"$7\"\\t\"$8}}' log/stat.{_rid}.txt > log/temp && mv log/temp log/stat.{_rid}.txt")
    with open(f"{CURRDIR}/log/stat.{_rid}.txt") as rid:
        best=rid.readline().split()
        if(len(best)==0):
            best=["-1"]
        best=best[0]
        BESTTEMP_builder("COVBLOAT",best)

def TMCMC(alpha,beta,k):
    #make identify_path and quantify_path
    rid=f"{realorcov}.{filter}.s{domgad_samplenum}.a{alpha}.b{beta}.k{k}.v3"
    os.system(f"rm -rf originscore identify_path quantify_path moss-out.{rid}")
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


    print(f"timeout -s 9 {TMCMC_TIMEOUT} {SEARCHBIN} -t {CURRDIR}/tmp/path_counted.txt -i {CURRDIR}/identify_path -s {CURRDIR}/tmp/sample_output -I {iternum} -m {domgad_samplenum} -F {CURRDIR}/programlist -L {CURRDIR}/lines -p {CURRDIR} -n {PROGNAME} -r {alpha} -w  {beta} -k {k} -q {quan_num} -S 2 &>{CURRDIR}/log/{rid}.txt")
    os.system(f"timeout -s 9 {TIMEOUT} {SEARCHBIN} -t {CURRDIR}/tmp/path_counted.txt -i {CURRDIR}/identify_path -s {CURRDIR}/tmp/sample_output -I {iternum} -m {domgad_samplenum} -F {CURRDIR}/programlist -L {CURRDIR}/lines -p {CURRDIR} -n {PROGNAME} -r {alpha} -w  {beta} -k {k} -q {quan_num} -S 2 >{CURRDIR}/log/{rid}.txt")
    
    time.sleep(2) # wait for sample_output
    subprocess.run(["cp",f"{CURRDIR}/tmp/sample_output",f"{CURRDIR}/moss-out.{rid}","-r"])
    subprocess.run(["/usr/local/bin/getLog.py",f"{CURRDIR}/log/{rid}.txt", f"{CURRDIR}/log/stat.{rid}.txt"])
    with open(f"{CURRDIR}/log/stat.{rid}.txt") as rid:
        best=rid.readline().split()
        if(len(best)==0):
            best=["-1"]
        best=best[0]
        BESTTEMP_builder("TMCMC",best)


if(not os.path.isdir(f"{CURRDIR}/log")):
    os.system(f"mkdir {CURRDIR}/log")


for k in ks:
    for alpha in alphas:
        for beta in betas:
            print(alpha,beta)
            os.system(f"su postgres -m -c ./generate_sql.py")

            print(f"rm `sample` files in src/ folder")
            os.system("find ./src/ -name '*.sample*.c' | xargs -n 1 rm")
            print(f"rm `origin` files in src/ folder")
            os.system(f"find ./src/ -name '*.origin.c' | xargs -n 1 rm")
            print(f"rm coverage_informations files in src/ folder")
            os.system(f"find ./src/ -name '*line.txt' | xargs -n 1 rm")
            os.system(f"find ./src/ -name '*bin.gcov' | xargs -n 1 rm")
#            os.system("find ./src/ -name '*.line.txt' | xargs -n 1 rm")

            print(f"cp /tmp/postgresql-12.14/src ./ -r") 
            os.system(f"cp /tmp/postgresql-12.14/src ./ -r") 
            print(f"chmod -R a+rw src")
            os.system(f"chmod -R a+rw src")

            print(f"-{CURRDIR}/path_generator/generate_cov.py", f"{CURRDIR}/pgsql/bin", COV, DOMGAD_DIR)
            subprocess.run([f"{CURRDIR}/path_generator/generate_cov.py", f"{CURRDIR}/pgsql/bin", COV, DOMGAD_DIR])

            os.system(f"chmod -R 777 backend bin common config contrib doc fe_utils include includetest interfaces pgsql pl port template test timezone tools tutorial")
            os.system(f"chown -R postgres:postgres backend bin common config contrib doc fe_utils include includetest interfaces pgsql pl port template test timezone tools tutorial")
            
            #region init envs and do some cleaning
            subprocess.run(" ".join(["rm","-rf","originscore","output.origin","inputfile","*BEST.c"]),shell=True)
            os.system(f"cp programlist tmp/")
            # subprocess.run(["source","/etc/profile"])
            #endregion init envs and do some cleaning

            if(version=="MOSS"):
                for subversion in ["TMCMC","COVBLOAT","DEBOP"]:
                    try:
                        if(subversion=="TMCMC"):
                            eval(subversion)(alpha,beta,k)
                        else:
                            eval(subversion)(f"{realorcov}.{filter}.s{debop_samplenum}.a{alpha}.b{beta}.k{k}.v{METHOD[subversion]}")
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
                        break
                    except subprocess.CalledProcessError as e:#error and need restart
                        print(e)
