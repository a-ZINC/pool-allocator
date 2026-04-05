#pragma once

#include <cstddef>
#include <iostream>
template<typename T, std::size_t N>
class PoolAllocator {
private:
    static constexpr std::size_t SLOT_SIZE = 
        sizeof(T) > sizeof(void*) ? sizeof(T) : sizeof(void*);

    static constexpr std::size_t ALIGNMENT =
        alignof(T) > alignof(void*) ? alignof(T) : alignof(void*);

    alignas(ALIGNMENT) unsigned char pool[N * SLOT_SIZE];

    void* freelist_head;
    std::size_t allocated;
    std::size_t peak;

    void* slot_ptr(std::size_t slotnumber) {
        return pool + slotnumber * SLOT_SIZE;
    }

    void write_next(void* slot, void* next) {
        *reinterpret_cast<void**>(slot) = next;
    }

    void* read_next(void* slot) {
        return *reinterpret_cast<void**>(slot);
    }

    void initialize_freelist() {
        for (std::size_t i=0; i<N-1; i++) {
            write_next(slot_ptr(i), slot_ptr(i+1));
        }
        write_next(slot_ptr(N-1), nullptr);
        freelist_head = slot_ptr(0);
    }

public:
    PoolAllocator():
        freelist_head(nullptr), allocated(0), peak(0) {
            initialize_freelist();
            std::cout << "After freelist init, head = " << freelist_head << std::endl;
            void* 
    }
    ~PoolAllocator() {
        std::cout << "Allocator destroyed" << std::endl;
    }
};
