#!/usr/bin/env python3.8
#cp /tmp/postgresql-12.14/src ./ -r && chmod -R a+rw src
import os
srclistfile="programlist"
srclist=[]
with open(srclistfile) as sl:
    srclist=[src[:-1] for src in sl.readlines()]

print(srclist)
for src in srclist:
    print(f"cp {src}.cov.origin.c {src}")
    os.system(f"cp {src}.cov.origin.c {src}")

os.system(f"chmod -R a+rw src")
