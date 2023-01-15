Final project for cs5600 (Computer Systems) at Northeastern University. 

Based on OSTEP suggested project: https://github.com/remzi-arpacidusseau/ostep-projects/tree/master/concurrency-pzip

This project was undertaken as an attempt to understand how to efficiently parallelize a process. The algorithm used for
zipping is RLE (run length encoding), which is only efficient for certain data (repititive), but is fairly simple and
easy to implement, allowing use to focuse on cocurrency. 

The idea was to create a zipping program that takes in file names, encodes those file using RLE algorithm, and writes
the encoding to stdout. Note that all files are written to stdout, so decoding will not be able to retrieve what data
belonged to what file. 

First, a linear/sequential zip was created (wzip.c), along with its unzip counterpart (unzip.c) to test for correctness. 

Next, an inefficient parallelization attempt (ineff.pzip.c) where every file is assigned a thread, discounting any
workload imbalance caused by different file sizes. 

Finally, a producer-consumer approach was implemented (pzip.c) to account for this potential imbalance, where files are
memory-mapped and split into 'pages' of bytes by a single 'producer' thread. Said page is then added to a buffer queue, 
where each page struct has referntial info, and 'consumer' threads all work in parallel on that queue until empty, where
they dequeue a 'page' and compress it (again using RLE algorithm), store it in a new output struct with the relevant
metadata. Once all threads are done, all  output structs are then accessed and used to write a single output stream to
stdout. 
