# SERVE Algorithm (v4)
### **S**IMD-**E**nhanced **R**ange & **V**alue **E**ngine

**SERVE** is a high-performance, hardware-aware search data structure written in C++. It is specifically designed to outperform standard library containers like `std::set` by leveraging modern CPU architectures, cache locality, and AVX2 SIMD instructions.



## 🚀 Key Features
* **SIMD Accelerated:** Uses 256-bit AVX2 registers to compare 8 integers simultaneously.
* **Cache-Optimized:** Features `alignas(64)` memory alignment to prevent cache-line splits and minimize latency.
* **Hybrid Search Engine:** A 3-stage lookup process:
    1. **Interpolation Search:** Predictive "best-guess" for uniform data.
    2. **SIMD Binary Search:** Hardware-parallelized narrowing of results.
    3. **Scalar Fallback:** Final high-precision identification.
* **Dynamic & Self-Balancing:** Supports real-time `insert()` and `remove()` operations with automatic block splitting and merging.
* **Range Queries:** Efficiently retrieve all elements within a `[low, high]` range.

## 📊 Performance Benchmark
The following results were captured using **Google Benchmark** (Clang 17, -O3, -mavx2).

| Data Size (N) | vs. `std::set` (Red-Black Tree) | Speedup |
| :--- | :--- | :--- |
| **100,000 Elements** | **3.2x Faster** | +220% |
| **1,000,000 Elements** | **~1.15x Faster** | Consistent Lead |

![100k Benchmark Result](benchmark_100k.png)
![1M Benchmark Result](benchmark_1m.png)

## 🛠 Why it's Faster
Traditional search structures like `std::set` rely on **Pointer Chasing**. The CPU has to jump to random RAM locations for every node, causing "Cache Misses." 

**SERVE** keeps data in contiguous, cache-aligned blocks. This allows the CPU's prefetcher to load data before the algorithm even asks for it, keeping the pipeline full and the execution at maximum speed.



## 💻 Getting Started

### Compilation
To unlock the hardware acceleration, you **must** use the `-mavx2` and `-O3` flags:

```bash
g++ -O3 -mavx2 main.cpp -o serve_app
