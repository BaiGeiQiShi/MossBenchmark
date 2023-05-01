#!/usr/local/bin/python3
import os
import re
testfolder = "testscript/kn/"
for f in os.listdir(testfolder):
    with open(testfolder+f,"r") as test:
        txt = test.read()
        testnum = re.findall("o\d+",txt)[0]
        txtlines = txt.split("\n")
    if(testnum[1:]==f[5:]):
        cmd = f'''sed -i -n '$d' $OUTDIR/{testnum}'''
        txtlines.insert(16,cmd)
        with open(testfolder+f,"w") as test:
            test.write("\n".join(txtlines))
    else:
        print(f)
        
