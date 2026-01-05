#ifndef SERVE_ALGORITHM_HPP
#define SERVE_ALGORITHM_HPP

#include <iostream>
#include <vector>
#include <algorithm>
#include <immintrin.h>
#include <utility>
#include <iterator>

/**
 * SERVE: SIMD-Enhanced Range & Value Engine
 * A hardware-aware, self-balancing search structure.
 */

constexpr int TARGET_BLOCK_SIZE = 4096;
constexpr int MAX_BLOCK_SIZE = 8192;
constexpr int MERGE_THRESHOLD = TARGET_BLOCK_SIZE / 2;

struct alignas(64) Block {
    int minVal = 0;
    int maxVal = 0;
    std::vector<int> data;

    Block() { data.reserve(MAX_BLOCK_SIZE); }

    inline bool contains(int x) const {
        return !data.empty() && x >= minVal && x <= maxVal;
    }

    inline bool search(int x) const {
        if (data.empty()) return false;
        size_t low = 0, high = data.size() - 1;

        // Stage 1: Interpolation
        for (int steps = 0; steps < 3; ++steps) {
            if (low > high) return false;
            if (data[low] == x || data[high] == x) return true;
            double pos = low + ((double)(x - data[low]) / (data[high] - data[low])) * (high - low);
            size_t mid = std::clamp((size_t)pos, low + 1, high - 1);
            if (data[mid] == x) return true;
            if (data[mid] < x) low = mid + 1;
            else high = mid - 1;
        }

        // Stage 2: SIMD (Safe Unaligned Load)
        #ifdef __AVX2__
        __m256i target_vec = _mm256_set1_epi32(x);
        while (high - low > 32) {
            size_t mid = low + (high - low) / 2;
            __m256i vals = _mm256_loadu_si256((__m256i*)(data.data() + mid));
            __m256i cmp = _mm256_cmpgt_epi32(target_vec, vals);
            int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp));
            if (mask == 0xFF) low = mid + 8;
            else high = mid + 8;
        }
        #endif

        // Stage 3: Scalar Fallback
        while (low <= high) {
            size_t mid = low + (high - low) / 2;
            if (data[mid] == x) return true;
            if (data[mid] < x) low = mid + 1;
            else high = mid - 1;
        }
        return false;
    }

    inline void insert(int x) {
        auto it = std::lower_bound(data.begin(), data.end(), x);
        if (it != data.end() && *it == x) return;
        data.insert(it, x);
        minVal = data.front();
        maxVal = data.back();
    }

    inline bool remove(int x) {
        auto it = std::lower_bound(data.begin(), data.end(), x);
        if (it == data.end() || *it != x) return false;
        data.erase(it);
        if (!data.empty()) {
            minVal = data.front();
            maxVal = data.back();
        }
        return true;
    }

    inline int size() const { return data.size(); }
};

class UltimateHybridSearch {
private:
    std::vector<Block> blocks;

    inline int findBlockContaining(int x) const {
        if (blocks.empty()) return -1;
        int left = 0, right = blocks.size() - 1, result = -1;
        while (left <= right) {
            int mid = left + (right - left) / 2;
            if (blocks[mid].maxVal < x) left = mid + 1;
            else {
                if (blocks[mid].contains(x)) result = mid;
                right = mid - 1;
            }
        }
        return result;
    }

    void splitBlockIfNeeded(int idx) {
        if (idx < 0 || idx >= (int)blocks.size() || blocks[idx].size() <= MAX_BLOCK_SIZE) return;
        Block& b = blocks[idx];
        int mid = b.size() / 2;
        Block right;
        right.data.assign(b.data.begin() + mid, b.data.end());
        b.data.resize(mid);
        b.minVal = b.data.front(); b.maxVal = b.data.back();
        right.minVal = right.data.front(); right.maxVal = right.data.back();
        blocks.insert(blocks.begin() + idx + 1, std::move(right));
    }

    void mergeBlocksIfNeeded(int idx) {
        if (idx < 0 || idx >= (int)blocks.size() - 1) return;
        if (blocks[idx].size() + blocks[idx+1].size() < TARGET_BLOCK_SIZE) {
            blocks[idx].data.insert(blocks[idx].data.end(), std::make_move_iterator(blocks[idx+1].data.begin()), std::make_move_iterator(blocks[idx+1].data.end()));
            blocks[idx].minVal = blocks[idx].data.front();
            blocks[idx].maxVal = blocks[idx].data.back();
            blocks.erase(blocks.begin() + idx + 1);
        }
    }

public:
    UltimateHybridSearch() { blocks.reserve(512); }

    void build(std::vector<int>& data) {
        blocks.clear();
        if (data.empty()) return;
        std::sort(data.begin(), data.end());
        data.erase(std::unique(data.begin(), data.end()), data.end());
        for (size_t i = 0; i < data.size(); i += TARGET_BLOCK_SIZE) {
            size_t end = std::min(i + (size_t)TARGET_BLOCK_SIZE, data.size());
            Block b;
            b.data.assign(data.begin() + i, data.begin() + end);
            b.minVal = b.data.front(); b.maxVal = b.data.back();
            blocks.push_back(std::move(b));
        }
    }

    bool query(int x) const {
        int idx = findBlockContaining(x);
        return idx >= 0 && blocks[idx].search(x);
    }

    void insert(int x) {
        if (blocks.empty()) {
            Block b; b.insert(x);
            blocks.push_back(std::move(b));
            return;
        }
        auto it = std::upper_bound(blocks.begin(), blocks.end(), x, [](int v, const Block& b){ return v < b.minVal; });
        int idx = (it == blocks.begin()) ? 0 : std::distance(blocks.begin(), --it);
        blocks[idx].insert(x);
        splitBlockIfNeeded(idx);
    }

    std::vector<int> rangeQuery(int low, int high) const {
        std::vector<int> res;
        auto it = std::lower_bound(blocks.begin(), blocks.end(), low, [](const Block& b, int v){ return b.maxVal < v; });
        for (; it != blocks.end() && it->minVal <= high; ++it) {
            auto start = std::lower_bound(it->data.begin(), it->data.end(), low);
            auto end = std::upper_bound(start, it->data.end(), high);
            res.insert(res.end(), start, end);
        }
        return res;
    }

    void printStats() const {
        std::cout << "Blocks: " << blocks.size() << " | Elements: " << getTotalElements() << std::endl;
    }

    size_t getTotalElements() const {
        size_t total = 0;
        for (const auto& b : blocks) total += b.size();
        return total;
    }
};

#endif
