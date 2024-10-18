# Common

启动容器需要添加flag： docker run --cap-add LINUX_IMMUTABLE --network=host --name mossmcr -it moss-mcr:0.1 bash

在目录 MossBenchmark/$PROGRAM/ 下新建 feat_config 文件夹，  

feat_config 为 Chisel 和 Razor 的输入文件夹，存放保留指定功能的配置文件  

feat_config 内的配置文件需符合以下格式：  

命名：  
config_a${alpha}.b${beta}.txt (例，config_a0.25.b0.3.txt)  
内容：  
...  
pass-kn-o${test_script_number}  
...  
pass-kn-o42  
fail-kn-o43  
...  

# Chisel
./run_chisel $PROGRAM $CHISEL_TIMEOUT (例，./run_chisel mkdir-5.2.1 5m )

输出：MossBenchmark/$PROGRAM/final_out/a0.\*.b0.\*/  

输出：MossBenchmark/$PROGRAM/chisel_final_score.csv

# Razor
输出：MossBenchmark/$PROGRAM/razor_code/reduced/a0.\*.b0.\*_(cov|covl1|covl2|covl3|covl4)/  

输出：MossBenchmark/$PROGRAM/razor_final_score.csv

