#include <iostream>
#include <vector>
#include <chrono>
#include "../include/pool-allocator.hpp"

constexpr std::size_t OPS = 1'000'000;
volatile long long sink = 0;

void benchmark_new_delete_bulk() {
    std::vector<int*> ptrs;
    ptrs.reserve(OPS);

    auto start = std::chrono::high_resolution_clock::now();

    for (std::size_t i = 0; i < OPS; i++) {
        int* p = new int(5);
        sink += *p;
        ptrs.push_back(p);
    }

    for (int* p : ptrs) {
        delete p;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "new/delete bulk: " << duration.count() << " us\n";
}

void benchmark_pool_bulk() {
    PoolAllocator<int, OPS> allocator;
    std::vector<int*> ptrs;
    ptrs.reserve(OPS);

    auto start = std::chrono::high_resolution_clock::now();

    for (std::size_t i = 0; i < OPS; i++) {
        int* p = allocator.construct(5);   // if your construct allocates internally
        if (!p) {
            std::cerr << "Pool out of memory\n";
            return;
        }
        sink += *p;
        ptrs.push_back(p);
    }

    for (int* p : ptrs) {
        allocator.destroy(p);
        allocator.deallocate(p);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "pool bulk: " << duration.count() << " us\n";
}

int main() {
    benchmark_new_delete_bulk();
    benchmark_pool_bulk();
    std::cout << "sink = " << sink << "\n";
    return 0;
}