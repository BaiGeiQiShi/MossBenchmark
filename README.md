# MossBenchmark
## chown

Before running chown, you must create a user ```mychown``` that belongs to a user group ```mychown```

## vim: 

    1.you need to download ncurses library and expect

    `apt install libncurses-dev expect parallel`

    2.you also need to set timeout more than 1 (in test.sh and generate_cov.py), because open file in vim cost some time.

    3.and please set a loose watchdog in covpath(in Moss/CovPath. Originally it's 30s). It's dangerous when less than 5min. I found 10min(300s) workable.

    4.vim use expect script, and it launch many vim-5.8 thread without tty. Take careful to close them. I 've use `cleanup` script in vim-5.8 directory to do this

    5. modify the `cleanup` path

## bash

you need to modify path of `lib` in `compile.sh` 

## make

1. you need to modify path of `lib` in `compile.sh`
2. `date-8.21` and `make-3.79` have crisis. `date-8.12` may change the system time which will be used in `make`'s testscripts. Don't run them together.

## mkdir

  Moss use `rsync` to remove garbage files in `/`
    `apt install rsync`
 
## gzip, bzip2, tar

  Moss use `chattr +/-i` to keep `/` and working dir unchanged

## postgresql-12.14

Use "/postgresql-12.14" (absolute path) as working directory, or you have to change path in `postgresql-12.14/path_generator/generate_cov.py` and `postgresql-12.14/cleanup` to your preference.

Due to the impact of long comment blocks on parsing code, we use `postgresql-12.14/deletecomments.py` to delete all comments in the source code. 

## In the debloating scripts:
- `METHOD`: 
- `PROGNAME`: 
- `version`: 
- `debop_samplenum`:
- `domgad_samplenum`:
- `alphas`:
- `ks`:
- `betas`:
- `CURRDIR`:
- `DEBOP_DIR`:
- `DOMGAD_DIR`:
- `COV`:
- `iternum`:
- `realorcov`:
