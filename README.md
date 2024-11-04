# MossBenchmark
This repository contains 26 programs used by Moss. If you want to know how to use this benchmark, please refer to [Moss](https://github.com/BaiGeiQiShi/Moss).

## 1. Adjustments to some programs
**Note:** The Dockerfile provided in [Moss](https://github.com/BaiGeiQiShi/Moss) already includes all the adjustments. Here is just a detailed explanation of each adjustment we made.

### 1.1 chown-8.2:
**Our adjustments:** Since docker containers default to the root user, some of the test cases for chown-8.2 involves operations related to the root user. To address this, we created a new user group and user named mychown, and adjusted the test cases to serve more effectively as oracles.

**What you need to do before debloating:** 

Before debloating chown-8.2, you must create a user ```mychown``` that belongs to a user group ```mychown```.

### 1.2 vim-5.8: 
**Our adjustments:** Due to subprocess issues, we were unable to fully trace all the code covered by the test cases in vim-5.8 using LLVM. After manual analysis, we found that it was because vim-5.8 creates the subprocess using fork at ```line 100313```, which prevented LLVM from successfully tracing the subprocess's code. Therefore, **we manually created a coverage version** of the vim-5.8 for debloating.

**What you need to do before debloating:** 

①You need to download ncurses library and expect.

`apt install libncurses-dev expect parallel`

②You need to set timeout more than 1 second (in test.sh and generate_cov.py), because open file in vim cost some time.

③Vim use expect script, and it launch many vim-5.8 thread without tty. Take careful to close them. We have used `cleanup` script in vim-5.8 directory to do this. Please modify the path in `cleanup`.

### 1.3 bash-2.05:
**Our adjustments:** We were also unable to correctly obtain the bash-2.05 code covered by all test cases using LLVM, but we cannot confirm which code needs to be added back through debugging. On one hand, debugging requires using `clang -g`, while Moss uses `clang -O3` during debloating. Therefore, just because bash-2.05 works normally with `clang -g` doesn't mean it will work correctly with `clang -O3`. On the other hand, since bash-2.05's test cases are interactively verified based on console output, we cannot use instrumentation for manual debugging. Consequently, we only use the 845 test cases that can be correctly handled by the coverage version of bash-2.05 to calculate the generality of bash-2.05.

**What you need to do before debloating:** 

You need to modify the path of `lib` in `compile.sh`.

### 1.4 make-3.79:
**Our adjustments:** None.

**What you need to do before debloating:** 

①You need to modify the path of `lib` in `compile.sh`.

②`date-8.21` and `make-3.79` have crisis. `date-8.12` may change the system time which is used in `make`'s testscripts. Don't run them together.

### 1.5 mkdir-5.2.1:
**Our adjustments:** We use `rsync` to remove garbage files in `/`, as the debloated program of mkdir-5.2.1 may generated lots of garbage files in `/`.

**What you need to do before debloating:** 

Before running mkdir-5.2.1, you must run `apt install rsync`.
 
### 1.6 gzip-1.2.4, gzip-1.3, bzip2-1.0.5, tar-1.14
**Our adjustments:**  We use `chattr +/-i` to keep `/bin`, `/usr`, and working directory unchanged while debloating, as the debloated programs of these four programs may mistakenly compress files in these folders.

**What you need to do before debloating:** 

None.

### 1.7 postgresql-12.14
**Our adjustments:** Due to the impact of long comment blocks on parsing code, we use `postgresql-12.14/deletecomments.py` to delete all comments in the source code. 

**What you need to do before debloating:**

Use "/postgresql-12.14" (absolute path) as working directory, or you need to change path in `postgresql-12.14/path_generator/generate_cov.py` and `postgresql-12.14/cleanup` to your path.

## 2. Key parameters in the debloating scripts:
- `PROGNAME`: The program name.
- `version`: The debloating strategy (3 is Moss, 2 is Moss-s1, 1 is Moss-s2, 0 is Moss-s3).
- `debop_samplenum`: The sample number of the second and third stages.
- `domgad_samplenum`: The sample number of the first stage.
- `alphas`: Alpha value (weight for attack surface reduction).
- `ks`: K value (for computing density values).
- `betas`: Beta value (weight for generality).
- `DEBOP_DIR`: The path of CovBlock_Stmt.
- `DOMGAD_DIR`: The path of CovPath.
- `COV`: The path of cov.
- `iternum`: The iteration number of each stage of Moss.
- `realorcov`: Use real or top program as the baseline.
