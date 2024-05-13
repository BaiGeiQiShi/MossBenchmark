#!/usr/bin/env python3.8
cov_files = []
with open("merged.bin.gcov") as merged:
    for fn in merged.readlines():
        if(fn.startswith("file:") and fn.endswith(".c\n")):
            cov_files.append(fn[5:-1])
with open("programlist", "w") as pl:
    print("\n".join(cov_files), file=pl)
