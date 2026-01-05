#include "serve.hpp"
#include <chrono>
#include <random>

int main() {
    const int N = 1000000;
    std::vector<int> data;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 10000000);

    for (int i = 0; i < N; ++i) data.push_back(dist(rng));

    UltimateHybridSearch serve;
    
    auto start = std::chrono::high_resolution_clock::now();
    serve.build(data);
    auto end = std::chrono::high_resolution_clock::now();
    
    std::cout << "Build time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
    serve.printStats();

    // Test Search
    int target = data[500];
    if (serve.query(target)) std::cout << "Found target: " << target << std::endl;

    return 0;
}
