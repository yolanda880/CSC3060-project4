#include "prefetcher.h"

std::vector<uint64_t> NextLinePrefetcher::calculatePrefetch(uint64_t current_addr, bool miss) {
    (void)current_addr;
    (void)miss;
    std::vector<uint64_t> prefetches;

    // TODO: Task 3
    // 1. Align current_addr down to the current cache block.
    // 2. Prefetch the next sequential block.
    uint64_t block_addr = current_addr & ~(block_size - 1);
    uint64_t next_block_addr = block_addr + block_size;
    prefetches.push_back(next_block_addr);
    return prefetches;
}

std::vector<uint64_t> StridePrefetcher::calculatePrefetch(uint64_t current_addr, bool miss) {
    (void)miss;
    std::vector<uint64_t> prefetches;
    uint64_t current_block = current_addr & ~(block_size - 1);
    
    if (has_last_block) {
        int64_t stride = (int64_t)(current_block - last_block) / block_size;
        
        if (stride == last_stride && stride != 0) {
            confidence++;
            if (confidence >= 2) {
                uint64_t next_block = current_block + stride * block_size;
                prefetches.push_back(next_block);
            }
        } else {
            last_stride = stride;
            confidence = 1;
        }
    }
    
    last_block = current_block;
    has_last_block = true;
    
    return prefetches;
}

Prefetcher* createPrefetcher(std::string name, uint32_t block_size) {
    if (name == "NextLine") return new NextLinePrefetcher(block_size);
    if (name == "Stride") return new StridePrefetcher(block_size);
    return new NoPrefetcher();
}
