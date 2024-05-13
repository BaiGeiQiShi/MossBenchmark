#!/usr/bin/env python3
import os, sys
import time

tests = os.listdir("./kn")

if os.path.exists("tmp"):
    os.system("rm -rf tmp/*")
else:
    os.system("mkdir tmp")
    os.system("chown postgres tmp && chgrp postgres tmp")

if os.path.exists("output"):
    os.system("rm -rf output/*")
else:
    os.system("mkdir output")
    os.system("chown postgres output && chgrp postgres output")

if os.path.exists("output.origin"):
    os.system("rm -rf output.origin/*")
else:
    os.system("mkdir output.origin")
    os.system("chown postgres output.origin && chgrp postgres output.origin")

BIN = "/postgresql-12.14/pgsql/bin"
OUTDIR = "/postgresql-12.14/testscript"
TIMEOUT = 10
INDIR = "/postgresql-12.14"

os.chdir("/postgresql-12.14/testscript/tmp")
start = time.time()
for test in tests:
    os.system("rm -rf ./*")
    result = os.system(f'/postgresql-12.14/testscript/kn/{test} {BIN} {OUTDIR}/output.origin {TIMEOUT} {INDIR}')
    if result != 0:
        exit(1)
end = time.time()
run_time_1 = end -start
start = time.time()
for test in tests:
    os.system("rm -rf ./*")
    result = os.system(f'/postgresql-12.14/testscript/kn/{test} {BIN} {OUTDIR}/output {TIMEOUT} {INDIR}')
    if result != 0:
        exit(1)
end = time.time()
run_time_2 = end -start
print(run_time_1)
print(run_time_2)

os.chdir("/postgresql-12.14/testscript/")
outs = os.listdir("/postgresql-12.14/testscript/output")
for out in outs:
    os.system(f"diff -q /postgresql-12.14/testscript/output/{out} /postgresql-12.14/testscript/output.origin/{out}")