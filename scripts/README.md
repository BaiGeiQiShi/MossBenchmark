# How to run Chisel and Razor
## Please note that:
①In our experiments, Chisel and Razor can only work properly on 22 programs except make-3.79, bash-2.05, vim-5.8, postgresql-12.14.
②The scripts in this folder are for backup only. The working directory of each program already contains these scripts.


## 1. Please create a new folder `feat_config` in MossBenchmark/$PROGRAM/.

①`feat_config` is the input folder for Chisel and Razor, storing configuration files that specify the test cases that need to be passed.

②The configuration file in `feat_config` must conform to the following format:

**File Name:**  
config_a${alpha}.b${beta}.txt (eg, `config_a0.25.b0.3.txt`)  

**Content:**  
...  
pass-kn-o${test_script_number}  
...  
pass-kn-o42  
fail-kn-o43  
...  

## 2. Run Chisel
```
cd MossBenchmark/$PROGRAM/
./run_chisel $PROGRAM $CHISEL_TIMEOUT (eg, ./run_chisel mkdir-5.2.1 5m)
```

Output：MossBenchmark/$PROGRAM/final_out/a0.\*.b0.\*/  

Output：MossBenchmark/$PROGRAM/chisel_final_score.csv

## 3. Run Razor
```
cd MossBenchmark/$PROGRAM/
./run_razor $PROGRAM (eg, ./run_razor mkdir-5.2.1)
```

Output：MossBenchmark/$PROGRAM/razor_code/reduced/a0.\*.b0.\*_(cov|covl1|covl2|covl3|covl4)/  

Output：MossBenchmark/$PROGRAM/razor_final_score.csv

