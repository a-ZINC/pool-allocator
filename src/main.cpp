#include<iostream>
#include "../include/pool-allocator.hpp"

int main() {
    PoolAllocator<int, 4> allocator;
    std::cout << "\n--- Allocating 1 ---\n";
    int* a = allocator.allocate();
    std::cout << "\n--- Allocating 2 ---\n";
    int* b = allocator.allocate();
    std::cout << "\n--- Allocating 3 ---\n";
    int* c = allocator.allocate();
    std::cout << "\n--- Allocating 4 ---\n";
    int* d = allocator.allocate();
    std::cout << "\n--- Allocating 5 (fail) ---\n";
    int* e = allocator.allocate();

    allocator.debug_state();
    allocator.debug_freelist();

    std::cout << "\n--- Deallocating 1 ---\n";
    allocator.deallocate(a);
    std::cout << "\n--- Deallocating 2 ---\n";
    allocator.deallocate(c);

    allocator.debug_state();
    allocator.debug_freelist();

    std::cout<< "\n--- Allocating again (use c) ---\n";
    int* f = allocator.allocate();
    std::cout << "\n---Allocating again (use a) ---\n";
    int* g = allocator.allocate();

    allocator.debug_state();
    allocator.debug_freelist();

    std::cout << "\nPointers:\n";
    std::cout << "a = " << a << std::endl;
    std::cout << "b = " << b << std::endl;
    std::cout << "c = " << c << std::endl;
    std::cout << "d = " << d << std::endl;
    std::cout << "e = " << e << std::endl;
    std::cout << "x = " << f << std::endl;
    std::cout << "y = " << g << std::endl;
    return 0;
}