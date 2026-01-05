/**
 * Ultimate Hybrid Search (UHS) v4
 * * A high-performance, hardware-aware search structure that combines 
 * the logic of B-Trees with the raw speed of SIMD and Cache-locality.
 * * Features:
 * - Cache-line Alignment (64-byte)
 * - Interpolation Search (for uniform data)
 * - SIMD Binary Search (AVX2 Hardware Acceleration)
 * - Block-based Memory Management
 */

#include <vector>
#include <algorithm>
#include <immintrin.h>
#include <iostream>

using namespace std;

// Tuning constants for modern CPU Cache sizes
constexpr int TARGET_BLOCK_SIZE = 4096; 
constexpr int MAX_BLOCK_SIZE    = 8192;
constexpr int MERGE_THRESHOLD   = TARGET_BLOCK_SIZE / 2;

/**
 * @brief Represents a contiguous segment of sorted data aligned to CPU cache lines.
 */
struct alignas(64) Block {
    int minVal = 0;
    int maxVal = 0;
    vector<int> data;

    Block() { 
        data.reserve(MAX_BLOCK_SIZE); 
    }

    /**
     * @brief Metadata check to see if a value could exist in this block.
     * Complexity: O(1)
     */
    inline bool contains(int target) const {
        return !data.empty() && target >= minVal && target <= maxVal;
    }

    /**
     * @brief Performs a multi-stage search: Interpolation -> SIMD -> Scalar.
     * Complexity: O(log B) where B is block size.
     */
    inline bool search(int target) const {
        if (data.empty()) return false;

        size_t low = 0;
        size_t high = data.size() - 1;

        // --- STAGE 1: Interpolation Search ---
        // Fast-track for uniformly distributed data.
        for (int steps = 0; steps < 3; ++steps) {
            if (low > high) return false;
            if (data[low] == target || data[high] == target) return true;

            // Estimate position: pos = low + (target - min) / (max - min) * range
            double pos = low + ((double)(target - data[low]) / (data[high] - data[low])) * (high - low);
            size_t mid = std::clamp((size_t)pos, low + 1, high - 1);

            if (data[mid] == target) return true;
            if (data[mid] < target) low = mid + 1;
            else high = mid - 1;
        }

        // --- STAGE 2: SIMD Binary Search (AVX2) ---
        // Compares 8 integers at once using 256-bit registers.
        #ifdef __AVX2__
        __m256i target_vec = _mm256_set1_epi32(target);
        while (high - low > 32) {
            size_t mid = low + (high - low) / 2;
            // Load 8 integers from memory (must be aligned or use loadu)
            __m256i vals = _mm256_loadu_si256((__m256i*)(data.data() + mid));
            __m256i cmp  = _mm256_cmpgt_epi32(target_vec, vals);
            int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp));
            
            if (mask == 0xFF) low = mid + 8; // Target is greater than all 8 values
            else high = mid + 8;
        }
        #endif

        // --- STAGE 3: Final Scalar Binary Search ---
        while (low <= high) {
            size_t mid = low + (high - low) / 2;
            if (data[mid] == target) return true;
            (data[mid] < target) ? low = mid + 1 : high = mid - 1;
        }
        return false;
    }
};

class UltimateHybridSearch {
private:
    vector<Block> blocks;

    /**
     * @brief Finds the index of the block that likely contains target.
     * Uses binary search over block metadata. Complexity: O(log K)
     */
    inline int locateBlock(int target) const {
        if (blocks.empty()) return -1;
        int left = 0, right = blocks.size() - 1;
        int result = -1;

        while (left <= right) {
            int mid = left + (right - left) / 2;
            if (blocks[mid].maxVal < target) {
                left = mid + 1;
            } else {
                if (blocks[mid].contains(target)) result = mid;
                right = mid - 1;
            }
        }
        return result;
    }

public:
    /**
     * @brief Constructs the initial structure from a raw vector.
     * Complexity: O(N log N) due to sorting.
     */
    void build(vector<int>& input_values) {
        blocks.clear();
        if (input_values.empty()) return;

        sort(input_values.begin(), input_values.end());
        input_values.erase(unique(input_values.begin(), input_values.end()), input_values.end());

        for (size_t i = 0; i < input_values.size(); i += TARGET_BLOCK_SIZE) {
            size_t end = min(i + (size_t)TARGET_BLOCK_SIZE, input_values.size());
            Block b;
            b.data.assign(input_values.begin() + i, input_values.begin() + end);
            b.minVal = b.data.front();
            b.maxVal = b.data.back();
            blocks.push_back(std::move(b));
        }
    }

    /**
     * @brief High-level search function.
     */
    bool query(int target) const {
        int idx = locateBlock(target);
        return (idx != -1) && blocks[idx].search(target);
    }
};
