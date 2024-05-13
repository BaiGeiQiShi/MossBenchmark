#!/usr/bin/env python3.8
import os
CURRDIR=os.getcwd()
v0logs=[]
for f in os.listdir("log"):
    if(not f.startswith("stat") and f.endswith("v0.txt")):
        v0logs.append(f[:-4])
for _rid in v0logs:
    print(_rid)
    print(f"sort -k 8 {CURRDIR}/moss-out.{_rid}/Simplifiedlog.txt -nrb | head -n 1 > log/stat.{_rid}.txt && cat {CURRDIR}/moss-out.{_rid}/Simplifiedlog.txt >> log/stat.{_rid}.txt && awk -F'\t' '{{print $2\"\\t\"$3\"\\t\"$4\"\\t\"$5\"\\t\"$6\"\\t\"$7\"\\t\"$8}}' log/stat.{_rid}.txt > log/temp && mv log/temp log/stat.{_rid}.txt")
    os.system(f"sort -k 8 {CURRDIR}/moss-out.{_rid}/Simplifiedlog.txt -nrb | head -n 1 > log/stat.{_rid}.txt && cat {CURRDIR}/moss-out.{_rid}/Simplifiedlog.txt >> log/stat.{_rid}.txt && awk -F'\t' '{{print $2\"\\t\"$3\"\\t\"$4\"\\t\"$5\"\\t\"$6\"\\t\"$7\"\\t\"$8}}' log/stat.{_rid}.txt > log/temp && mv log/temp log/stat.{_rid}.txt")
