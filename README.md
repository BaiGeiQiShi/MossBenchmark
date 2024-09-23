# MossBenchmark

## 1.Adjustments to some programs
### 1.1 chown-8.2:
Before running chown, you must create a user ```mychown``` that belongs to a user group ```mychown```.

### 1.2 vim-5.8: 
①You need to download ncurses library and expect.

`apt install libncurses-dev expect parallel`

②You need to set timeout more than 1 second (in test.sh and generate_cov.py), because open file in vim cost some time.

③Vim use expect script, and it launch many vim-5.8 thread without tty. Take careful to close them. We have used `cleanup` script in vim-5.8 directory to do this. Please modify the path in `cleanup`.

### 1.3 bash-2.05:

You need to modify the path of `lib` in `compile.sh` 

### 1.4 make-3.79:

①You need to modify the path of `lib` in `compile.sh`

②`date-8.21` and `make-3.79` have crisis. `date-8.12` may change the system time which is used in `make`'s testscripts. Don't run them together.

### 1.5 mkdir-5.2.1:

Moss use `rsync` to remove garbage files in `/`.

`apt install rsync`
 
### 1.6 gzip-1.2.4, gzip-1.3, bzip2-1.0.5, tar-1.14

Moss use `chattr +/-i` to keep `/bin`, `/usr`, and working directory unchanged.

### 1.7 postgresql-12.14

①Use "/postgresql-12.14" (absolute path) as working directory, or you need to change path in `postgresql-12.14/path_generator/generate_cov.py` and `postgresql-12.14/cleanup` to your path.

②Due to the impact of long comment blocks on parsing code, we use `postgresql-12.14/deletecomments.py` to delete all comments in the source code. 

## 2. Key parameters in the debloating scripts:
- `PROGNAME`: The program name
- `version`: The debloating strategy. (3 is Moss, 2 is Moss-s1, 1 is Moss-s2, 0 is Moss-s3)
- `debop_samplenum`: The sample number of the second and third stages
- `domgad_samplenum`: The sample number of the first stage
- `alphas`: Alpha value (weight for attack surface reduction)
- `ks`: K value (for computing density values)
- `betas`: Beta value (weight for generality)
- `DEBOP_DIR`: The path of CovBlock_Stmt
- `DOMGAD_DIR`: The path of CovPath
- `COV`: The path of cov
- `iternum`: The iteration number of each stage of Moss
- `realorcov`: Use real or top program as the baseline
