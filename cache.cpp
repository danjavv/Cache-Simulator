#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <sstream>

using namespace std;

// Constants
long long L1_SIZE = 1024;         // L1 cache size in bytes
long long L2_SIZE = 65536;        // L2 cache size in bytes
long long BLOCK_SIZE = 64;        // Block size in bytes
long long L1_ASSOC = 2;           // L1 associativity
long long L2_ASSOC = 8;           // L2 associativity
long long L1_LATENCY = 1;
long long L2_LATENCY = 20;        // L2 latency in cycles
long long DRAM_LATENCY = 200;     // DRAM latency in cycles

// Cycle count
long long cycle = 0;
long long L1_READS =0;
long long L1_WRITES =0;
long long L1_READMISSES=0;
long long L1_WRITEMISSES=0;
long long WB_FROM_L1MEM=0;
long long L2_READS=0;
long long L2_READMISSES=0;
long long L2_WRITES=0;
long long L2_WRITEMISSES=0;
long long WB_FROM_L2MEM=0;
long long c=0;
// Cache block structure
struct CacheBlock {
    bool valid;
    bool dirty;
    long long tag;
    long long last_used;
};

// Cache set structure
struct CacheSet {
    vector<CacheBlock> blocks;
};

class Cache {
public:
    long long size;
    long long block_size;
    long long associativity;
    long long num_sets;
    long long index_bits;
    long long tag_bits;
    vector<CacheSet> sets;

    Cache(long long size, long long block_size, long long associativity) {
        this->size = size;
        this->block_size = block_size;
        this->associativity = associativity;

        // Calculate number of sets
        this->num_sets = size / (block_size * associativity);

        // Calculate number of index bits and tag bits
        this->index_bits = log2(num_sets);
        this->tag_bits = 32 - index_bits - log2(block_size);

        // Initialize cache sets and blocks
        sets.resize(num_sets);
        for (long long i = 0; i < num_sets; i++) {
            sets[i].blocks.resize(associativity);
            for (long long j = 0; j < associativity; j++) {
                sets[i].blocks[j].valid = false;
                sets[i].blocks[j].dirty = false;
                sets[i].blocks[j].tag = -1;
                sets[i].blocks[j].last_used = 0;
            }
        }
    }

    // Read from cache
    bool read(unsigned long long addr) {
        long long index = (addr / block_size) % num_sets;
        long long tag = addr / (block_size * num_sets);
        //cout <<addr<<" "<<index<<" "<<tag<<"\n";
        for (long long i = 0; i < associativity; i++) {
            CacheBlock& block = sets[index].blocks[i];
            if (block.valid && block.tag == tag) {
                // Cache hit
                block.last_used = cycle;
                return true;
            }
        }
        // Cache miss
        return false;
    }

    // Write to cache
    void write(unsigned long long addr, bool rd, Cache& l2_cache) {
        //cout<<addr<<" "<<block_size<<" "<<num_sets<<"\n";
        long long index = (addr / block_size) % num_sets;
        long long tag = addr / (block_size * num_sets);
        //cout<<addr-(tag*num_sets+index)*block_size<<"\n";
        //cout<<index<<index_bits+tag<<tag_bits<<"\n";
        for (long long i = 0; i < associativity; i++) {
            CacheBlock& block = sets[index].blocks[i];
            if (block.valid && block.tag == tag) {
                //cout<<"Block found\n";
                // Cache hit, mark block as dirty
                if(rd==0){
                    block.dirty = true;
                    //cout<<"Dirty bit set\n";
                } 
                block.last_used = cycle;
                return;
            }
        }

     //Write miss
    if(rd==0){
        if(lower_cache!=nullptr){
            L1_WRITEMISSES++;
            L2_READS++;
            cycle+=L2_LATENCY;
            if(!l2_cache.read(addr)){
                L2_READMISSES++;
                //cout<<"L2 cache miss, access DRAM\n";
                // L2 cache miss, access DRAM
                cycle += DRAM_LATENCY;
                l2_cache.write(addr,1,l2_cache);
            }
        }
        else{
            L2_WRITEMISSES++;
            cycle+=DRAM_LATENCY;
        }
    }

    for (long long i = 0; i < associativity; i++) {
        if (!sets[index].blocks[i].valid) {
            // Found invalid block, use it
            sets[index].blocks[i].valid = true;
            //cout<<"Found invalid block\n";
            if(rd==0){
                sets[index].blocks[i].dirty = true;
                //cout<<"Dirty bit set\n";
            }
            sets[index].blocks[i].tag = tag;
            sets[index].blocks[i].last_used = cycle;
            return;
        }
    }
           
    // Cache miss, evict LRU block
    long long min_last_used = cycle;
    long long min_last_used_idx = cycle;

    for (long long i = 0; i < associativity; i++) {
        if (sets[index].blocks[i].last_used < min_last_used) {
            // Found LRU block
            min_last_used = sets[index].blocks[i].last_used;
            min_last_used_idx = i;
        }
    }

    // Evict LRU block
    CacheBlock& block = sets[index].blocks[min_last_used_idx];
    if(block.dirty){
        //cout<<"Writing back\n";
        //Write backs from l2 to mem
        if(lower_cache==nullptr){
            WB_FROM_L2MEM++;
            cycle+=DRAM_LATENCY;
        }
        //Write backs from l1 to
        else{
            WB_FROM_L1MEM++;
            L2_WRITES++;
            cycle+=L2_LATENCY;
            lower_cache->write_back((block.tag * num_sets + index) * block_size);
        }
    }
    else{
        //cout<<"No writing back\n";
    }
    block.valid = true;
    if(rd==0){
        block.dirty = true;
        //cout<<"Dirty bit set\n";
    }
    else
    block.dirty = false;
    block.tag = tag;
    block.last_used = cycle;
}
void write_back(unsigned long long block_addr){
    // check if present in l2
    // update cycle += L2_LATENCY and mark dirty = true
    // else update cycle += DRAM_LATENCY
    long long index = (block_addr / block_size) % num_sets;
    long long tag = block_addr / (block_size * num_sets);
    //cout<<"Write back in L2\n";
    for (long long i = 0; i < associativity; i++) {
        CacheBlock& block = sets[index].blocks[i];
        if (block.valid && block.tag == tag) {
            // Cache hit, mark block as dirty
            //cout<<"Block wrote back in L2\n";
            block.dirty = true;
            block.last_used = cycle;
            return;
        }
    }
    //cout<<"Block not found in L2, block wrote back in DRAM\n";
    WB_FROM_L2MEM++;
    cycle+=DRAM_LATENCY;
}
// Flush cache
void flush(Cache& l3_cache) {
    for (long long i = 0; i < num_sets; i++) {
        for (long long j = 0; j < associativity; j++) {
            CacheBlock& block = sets[i].blocks[j];
            if (block.valid && block.dirty) {
                // Write dirty block to lower level cache
                unsigned long long addr = (block.tag * num_sets + i) * block_size;
                if(lower_cache!=nullptr)
                lower_cache->write(addr,1,l3_cache);
                block.dirty = false;
            }
        }
    }
}

// Set lower level cache
void set_lower_cache(Cache* lower_cache) {
    this->lower_cache = lower_cache;
}
private:
Cache* lower_cache;
};

// L1 and L2 caches
//Cache l1_cache(L1_SIZE, BLOCK_SIZE, L1_ASSOC);
//Cache l2_cache(L2_SIZE, BLOCK_SIZE, L2_ASSOC);
//Cache l3_cache(L2_SIZE, BLOCK_SIZE, L2_ASSOC);

int main(int argc, char* argv[]) {

if (argc != 7) {
    std::cerr << "Invalid number of arguments!" << std::endl;
    return 1;
}

BLOCK_SIZE = std::stoll(argv[1]);
L1_SIZE = std::stoll(argv[2]);
L1_ASSOC = std::stoll(argv[3]);
L2_SIZE = std::stoi(argv[4]);
L2_ASSOC = std::stoi(argv[5]);
std::string traceFile = argv[6];

Cache l1_cache(L1_SIZE, BLOCK_SIZE, L1_ASSOC);
Cache l2_cache(L2_SIZE, BLOCK_SIZE, L2_ASSOC);
Cache l3_cache(L2_SIZE, BLOCK_SIZE, L2_ASSOC);

// Set L1 and L2 caches
l1_cache.set_lower_cache(&l2_cache);
l2_cache.set_lower_cache(nullptr);

// Open trace file
ifstream trace_file(traceFile);

std::string line;
while (getline(trace_file, line)) {
    //cout<<line<<"\n";
    std::istringstream iss(line);
    char rw;
    unsigned long long addr;
    if (iss >> rw >> std::hex >> addr) {
        // Access L1 cache
        if (rw == 'r') {
            //cout << "Read: " << addr << "\n";
            L1_READS++;
            cycle+=L1_LATENCY;
            if (!l1_cache.read(addr)) {
                L1_READMISSES++;
                l1_cache.write(addr,1,l2_cache);
                // L1 cache miss, access L2 cache
                //cout<<"L1 cache miss, access L2 cache\n";
                cycle += L2_LATENCY;
                L2_READS++;
                if (!l2_cache.read(addr)) {
                    L2_READMISSES++;
                    //cout<<"L2 cache miss, access DRAM\n";
                    // L2 cache miss, access DRAM
                    cycle += DRAM_LATENCY;
                    l2_cache.write(addr,1,l3_cache);
                }
                else{
                    //cout<<"L2 cache hit\n";
                }
            }
            else{
                //cout<<"L1 cache hit\n";
            }
        } else {
            L1_WRITES++;
            cycle+=L1_LATENCY;
            //cout << "Write: " << addr << "\n";
            l1_cache.write(addr,0,l2_cache);
        }
       //cycle++;
    }
}
//cout<<"SEX!\n";
// Prlong long cycle count
cout<<"\n";
cout << "===== Simulation Results =====\n";
cout << "Total time taken: " << cycle <<" ns"<<endl;
cout << "L1 reads: "<<L1_READS<< endl;
cout << "L1 read misses: "<<L1_READMISSES<< endl;
cout << "L1 writes: "<<L1_WRITES<< endl;
cout << "L1 write misses: "<<L1_WRITEMISSES<< endl;
cout << "L1 miss rate: "<<((float)L1_READMISSES+(float)L1_WRITEMISSES)/(L1_READS+L1_WRITES) <<endl;
cout << "Writebacks from L1 to mem: "<<WB_FROM_L1MEM<<endl;
cout << "L2 reads: "<<L2_READS<< endl;
cout << "L2 read misses: "<<L2_READMISSES<< endl;
cout << "L2 writes: "<<L2_WRITES<< endl;
cout << "L2 write misses: "<<L2_WRITEMISSES<< endl;
cout << "L2 miss rate: "<<((float)L2_READMISSES+(float)L2_WRITEMISSES)/(L2_READS+L2_WRITES)<<endl;
cout << "Writebacks from L2 to mem: "<<WB_FROM_L2MEM<<endl;
cout<<"\n";
// Flush L1 and L2 caches
l1_cache.flush(l3_cache);
l2_cache.flush(l3_cache);
return 0;
}

//This implementation reads trace files and assigns read/write requests to the L1 cache. If a cache miss occurs in the L1 cache, the L2 cache is accessed. If a cache miss occurs in the L2 cache, the DRAM is accessed. The caches are flushed at the end of the simulation, and the cycle count is prlong longed. Note that this is a simplified implementation and does not handle all possible corner cases.





