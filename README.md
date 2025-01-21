# Cache-Simulator

Assignment 3 of course Computer Architecture at IIT Delhi - https://www.cse.iitd.ac.in/~rijurekha/col216/assignment3.pdf

N-way set associative cache simulator in C++. The simulator implements 2 level caches, L1 and L2. 
Simulator reads trace files and assigns requests to the L1 cache. L1 cache sends read/write requests to the L2 cache. 
L2 cache interacts with DRAM. L1 and L2 caches keep track of their own counters i.e. reads, writes, misses, hits etc. At the end of
the simulation, for a given trace file, this program prints the stats for both caches on the console.

1. Run make to create cache_simulate executable
2. Execute the runnable with example command :- ./cache_simulate 64 1024 2 65536 8 memory_trace_files/trace1.txt
3. The parameters are respectively:- BLOCKSIZE, L1_SIZE, L1_ASSOC, L2_SIZE, L2_ASSOC and (a trace file from https://www.cse.iitd.ac.in/~rijurekha/col216/memory_trace_files.tar.gz)
