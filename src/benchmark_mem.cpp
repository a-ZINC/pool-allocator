#include<iostream>
#include<cstdlib>
#include<vector>
#include<chrono>
#include "../include/pool-allocator.hpp"

constexpr std::size_t OPS = 1'000'000;

void benchmark_malloc_free() {
    auto start = std::chrono::high_resolution_clock::now();

    for (std::size_t i=0; i<OPS; i++) {
        void* ptr = std::malloc(sizeof(int));
        if (ptr == nullptr) {
            std::cerr << "Out of memory ammloc" << std::endl;
            return;
        }
        std::free(ptr);
    }


    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "malloc_free: " << duration.count() << "ms" << std::endl;

}

void benchmark_pool() {
    PoolAllocator<int, OPS> allocator;
    auto start = std::chrono::high_resolution_clock::now();

    for (std::size_t i=0; i<OPS; i++) {
        int* ptr = allocator.construct(0);
        if (ptr == nullptr) {
            std::cerr << "Out of memory pool" << std::endl;
            return;
        }
        allocator.deallocate(ptr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "pool: " << duration.count() << "ms" << std::endl;
}

int main() {
    benchmark_malloc_free();
    benchmark_pool();
    return 0;
}