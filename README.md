# MossBenchmark
1.Before running ```chown```, you must create a user ```mychown``` that belongs to a user group ```mychown```
2.After running ```mkdir```, Moss will use ```rsync``` to remove garbage files in ```/```
3.When debloating ```gzip``` ```bzip``` ```tar```, Moss will use ```chattr +i``` to keep ```/``` and it's working dir unchanged
4.```Make``` and ```Date``` cannot be debloated in the same Environment at the same time because ```Date``` may changed the system time and make will read the system time.
