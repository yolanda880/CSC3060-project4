#include "memory_hierarchy.h"
#include "prefetcher.h"
#include "repl_policy.h"
#include <cmath>
#include <iomanip>
#include <iostream>

using namespace std;

MainMemory::MainMemory(int lat) : latency(lat) {}

int MainMemory::access(uint64_t addr, char type, uint64_t cycle) {
    (void)addr;
    (void)type;
    (void)cycle;
    access_count++;
    return latency;
}

void MainMemory::printStats() {
    cout << "  [Main Memory] Total Accesses: " << access_count << endl;
}

CacheLevel::CacheLevel(string name, CacheConfig cfg, MemoryObject* next)
    : level_name(name), config(cfg), next_level(next) {
    policy = createReplacementPolicy(config.policy_name);
    prefetcher = createPrefetcher(config.prefetcher, config.block_size);

    uint64_t total_bytes = (uint64_t)config.size_kb * 1024;
    num_sets = total_bytes / (config.block_size * config.associativity);

    offset_bits = log2(config.block_size);
    index_bits = log2(num_sets);

    sets.resize(num_sets, vector<CacheLine>(config.associativity));

    cout << "Constructed " << level_name << ": "
         << config.size_kb << "KB, " << config.associativity << "-way, "
         << config.latency << "cyc, "
         << "[" << config.policy_name << " + " << prefetcher->getName() << "]" << endl;
}

CacheLevel::~CacheLevel() {
    delete policy;
    delete prefetcher;
}

uint64_t CacheLevel::get_index(uint64_t addr) {
    // TODO: Task 1
    // Compute the set index from the address.
    // Hint: remove block offset bits first, then keep only the index bits.
    uint64_t cache_size = (uint64_t)config.size_kb * 1024;
    uint64_t num_sets = cache_size / (config.block_size * config.associativity);
    uint64_t offset_bits = log2(config.block_size);
    uint64_t index_bits = log2(num_sets);
    uint64_t index = (addr >> offset_bits) & ((1 << index_bits) - 1);
    return index;
}

uint64_t CacheLevel::get_tag(uint64_t addr) {
    // TODO: Task 1
    // Compute the tag from the address.
    // Hint: shift away both block offset bits and set index bits.
    uint64_t cache_size = (uint64_t)config.size_kb * 1024;
    uint64_t num_sets = cache_size / (config.block_size * config.associativity);
    uint64_t offset_bits = log2(config.block_size);
    uint64_t index_bits = log2(num_sets);
    uint64_t tag = addr >> (offset_bits + index_bits);
    return tag;
}

uint64_t CacheLevel::reconstruct_addr(uint64_t tag, uint64_t index) {
    // TODO: Task 1 / Task 2
    // Rebuild a block-aligned address from a tag and set index.
    // This helper is useful when writing back an evicted dirty line.
    uint64_t cache_size = (uint64_t)config.size_kb * 1024;
    uint64_t num_sets = cache_size / (config.block_size * config.associativity);
    uint64_t offset_bits = log2(config.block_size);
    uint64_t index_bits = log2(num_sets);
    uint64_t addr = (tag << (index_bits + offset_bits)) | (index << offset_bits);
    return addr;
}

void CacheLevel::write_back_victim(const CacheLine& line, uint64_t index, uint64_t cycle) {
    // TODO: Task 1 / Task 2
    // Move dirty write-back logic into this helper.
    // Suggested steps:
    // 1. If the victim is not dirty, return immediately.
    // 2. If there is no next level, return immediately.
    // 3. Increment the write-back counter.
    // 4. Reconstruct the evicted block address from tag + index.
    // 5. Send a write access to the next level.
    if (!line.dirty || next_level == nullptr) {
            return;
        }
    write_backs++;
    uint64_t evicted_addr = reconstruct_addr(line.tag, index);
    next_level->access(evicted_addr, 'w', cycle);
}

int CacheLevel::access(uint64_t addr, char type, uint64_t cycle) {
    int lat = config.latency;
    bool is_write = (type == 'w');
    
    uint64_t index = get_index(addr);
    uint64_t tag = get_tag(addr);

    std::vector<CacheLine>& lines = sets[index];

    for(size_t way = 0; way < config.associativity; way++) {
        CacheLine& line = lines[way];
        if(line.valid && line.tag == tag) {
            hits++;
            policy->onHit(lines, way, cycle);
            if(is_write) {
                line.dirty = true;
            }
            if(line.is_prefetched) {
                line.is_prefetched = false;
            }
            if (prefetcher != nullptr) {
                std::vector<uint64_t> pf_addrs = prefetcher->calculatePrefetch(addr, true); 
                for (uint64_t pf_addr : pf_addrs) {
                    install_prefetch(pf_addr, cycle + lat);
                }
            }
            return lat;
        }
    }
    misses++;
    
    int victim_way = -1;
    for(size_t way = 0; way < config.associativity; way++) {
        if(!lines[way].valid) {
            victim_way = way;
            break;
        }
    }

    if (victim_way == -1){
        victim_way = policy->getVictim(lines);
    }

    CacheLine& victim_line = lines[victim_way];

    if (victim_line.valid && victim_line.dirty) {
        write_back_victim(victim_line, index, cycle + lat);
    }

    if (next_level != nullptr) {
        uint64_t block_addr = reconstruct_addr(tag, index);
        lat += next_level->access(block_addr, 'r', cycle + lat);
    }

    victim_line.valid = true;
    victim_line.tag = tag;
    victim_line.dirty = is_write;
    victim_line.is_prefetched = false;

    policy->onMiss(lines, victim_way, cycle);
    
    if (prefetcher != nullptr) {
        std::vector<uint64_t> pf_addrs = prefetcher->calculatePrefetch(addr, true);
        for (uint64_t pf_addr : pf_addrs) {
            install_prefetch(pf_addr, cycle + lat);
        }
    }
    return lat;
    
    
}

void CacheLevel::install_prefetch(uint64_t addr, uint64_t cycle) {
    uint64_t tag = get_tag(addr);
    uint64_t index = get_index(addr);
    
    if (index >= sets.size()) {
        return;
    }
    
    std::vector<CacheLine>& lines = sets[index];
    
    for (size_t way = 0; way < config.associativity; way++) {
        if (lines[way].valid && lines[way].tag == tag) {
            return;
        }
    }
    
    int victim_way = -1;
    
    for (size_t way = 0; way < config.associativity; way++) {
        if (!lines[way].valid) {
            victim_way = way;
            break;
        }
    }
    
    if (victim_way == -1) {
        victim_way = policy->getVictim(lines);
        
        if (lines[victim_way].valid && lines[victim_way].dirty) {
            write_back_victim(lines[victim_way], index, cycle);
        }
    }
    
    uint64_t block_addr = reconstruct_addr(tag, index);
    if (next_level != nullptr) {
        next_level->access(block_addr, 'r', cycle);
    }
    
    lines[victim_way].valid = true;
    lines[victim_way].tag = tag;
    lines[victim_way].dirty = false;     
    lines[victim_way].is_prefetched = true; 
    
    policy->onMiss(lines, victim_way, cycle);
    
    prefetch_issued++;
}

void CacheLevel::printStats() {
    uint64_t total = hits + misses;
    cout << "  [" << level_name << "] "
         << "Hit Rate: " << fixed << setprecision(2) << (total ? (double)hits / total * 100.0 : 0) << "% "
         << "(Access: " << total << ", Miss: " << misses << ", WB: " << write_backs << ")" << endl;
    cout << "      Prefetches Issued: " << prefetch_issued << endl;
}
