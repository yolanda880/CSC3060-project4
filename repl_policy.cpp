#include "repl_policy.h"
#include <climits>

// =========================================================
// TODO: Task 1 / Task 3 replacement policies
// Implement LRU first, then extend with SRRIP / BIP.
// =========================================================

void LRUPolicy::onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    // TODO: initialize a newly inserted line as MRU.
    set[way].last_access = cycle;
}

void LRUPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    // TODO: initialize a newly inserted line as MRU.
    set[way].last_access = cycle;
}

int LRUPolicy::getVictim(std::vector<CacheLine>& set) {
    // TODO: return the least recently used way.
    int victim = 0;
    uint64_t oldest = set[0].last_access;
    for(size_t way = 1; way < set.size(); way++) {
        if(set[way].last_access < oldest) {
            victim = way;
            oldest = set[way].last_access;
        }
    }
    return victim;

}



void SRRIPPolicy::onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    (void)cycle;
    set[way].rrpv = 0;
}

void SRRIPPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    (void)cycle;
    set[way].rrpv = 2;
}

int SRRIPPolicy::getVictim(std::vector<CacheLine>& set) {
    const int MAX_RRPV = 3;
    int assoc = set.size();
    
    while (true) {
        for (int i = 0; i < assoc; i++) {
            if (set[i].rrpv == MAX_RRPV) {
                return i;
            }
        }
        for (int i = 0; i < assoc; i++) {
            set[i].rrpv++;
        }
    }
}

void BIPPolicy::onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    set[way].last_access = cycle;
}

void BIPPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    (void)throttle;
    int assoc = set.size();
    insertion_counter++;
    if (insertion_counter % (2 * assoc) == 0) {
        set[way].last_access = cycle;
    } else {
        uint64_t min_time = UINT64_MAX;
        int lru_way = 0;
        for (int i = 0; i < assoc; i++) {
            if (set[i].last_access < min_time) {
                min_time = set[i].last_access;
                lru_way = i;
            }
        }
        set[way].last_access = set[lru_way].last_access;
    }
}

int BIPPolicy::getVictim(std::vector<CacheLine>& set) {
   int victim = 0;
    uint64_t oldest = set[0].last_access;
    for (size_t way = 1; way < set.size(); way++) {
        if (set[way].last_access < oldest) {
            victim = way;
            oldest = set[way].last_access;
        }
    }
    return victim;
}

ReplacementPolicy* createReplacementPolicy(std::string name) {
    if (name == "SRRIP") return new SRRIPPolicy();
    if (name == "BIP") return new BIPPolicy();
    return new LRUPolicy();
}
