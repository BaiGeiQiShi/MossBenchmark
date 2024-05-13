#!/usr/bin/python

import os
import re

path = "./src/test/regress/input"
files = os.listdir(path)

inputdir = os.path.abspath("./src/test/regress")
outputdir = os.path.abspath("./src/test/regress")
testtablespace = outputdir + "/testtablespace"
dlpath = os.path.abspath("./src/test/regress")
DLSUFFIX=".so"

if not os.path.exists(testtablespace):
    os.mkdir(testtablespace)

for oneFile in files:
    if not oneFile.endswith(".source"):
        continue
    fileHandler = open(("%s/%s" % (path, oneFile)), "r")
    listOfLines  =  fileHandler.readlines()
    fileHandler.close()
    with open(("./src/test/regress/sql/%s.sql" % oneFile.split(".")[0]), "w") as f:
        for line in listOfLines:
            line = line.replace("@abs_srcdir@", inputdir);
            line = line.replace("@abs_builddir@", outputdir);
            line = line.replace("@testtablespace@", testtablespace);
            line = line.replace("@libdir@", dlpath);
            line = line.replace("@DLSUFFIX@", DLSUFFIX);
            f.write(line)