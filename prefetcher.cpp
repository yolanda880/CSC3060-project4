#include "prefetcher.h"

std::vector<uint64_t> NextLinePrefetcher::calculatePrefetch(uint64_t current_addr, bool miss) {
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

    uint64_t current_block = current_addr & ~(uint64_t(block_size) - 1);
    int64_t current_block_num = static_cast<int64_t>(current_block / block_size);

    if (has_last_block) {
        int64_t last_block_num = static_cast<int64_t>(last_block / block_size);
        int64_t stride = current_block_num - last_block_num;

        if (stride == last_stride && stride != 0) {
            confidence++;
            if (confidence >= 2) {
                int64_t next_block_num = current_block_num + stride;
                if (next_block_num >= 0) {
                    uint64_t next_block = static_cast<uint64_t>(next_block_num) * uint64_t(block_size);
                    prefetches.push_back(next_block);
                }
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
