# MossBenchmark
Before running chown, you must create a user ```mychown``` that belongs to a user group ```mychown```

vim: 
  1.you need to download ncurses library and expect
  apt install libncurses-dev expect parallel

  2.you also need to set timeout more than 1 (in test.sh and generate_cov.py), because open file in vim cost some time.
  3.and please set a loose watchdog in covpath(in Moss/CovPath. Originally it's 30s). It's dangerous when less than 5min. I found 10min(300s) workable.
  4.vim use expect script, and it launch many vim-5.8 thread without tty. Take careful to close them.
    use cleanup script in vim-5.8 directory

bash
you need to modify path of `lib` in compile.sh 
make
you need to modify path of `lib` in compile.sh
