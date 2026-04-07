#pragma once

#include <cstddef>
#include <iostream>
#include<new>
#include<utility>
template<typename T, std::size_t N>
class PoolAllocator {
private:
    static_assert(N > 0, "N must be greater than 0");
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
            // std::cout << "After freelist init, head = " << freelist_head << std::endl;

            void* curr = freelist_head;
            std::size_t index = 0;

            while (curr != nullptr) {
                // std::cout << "Free node " << index
                        // << ": current = " << curr
                        // << ", next = " << read_next(curr)
                        // << std::endl;
                curr = read_next(curr);
                ++index;
            }
    }
    ~PoolAllocator() {
        // std::cout << "Allocator destroyed" << std::endl;
    }

    T* allocate() {
        if (freelist_head == nullptr) {
            // std::cout << "Out of memory" << std::endl;
            return nullptr;
        }

        void* curr_free = freelist_head;
        freelist_head = read_next(curr_free);

        ++allocated;
        if (allocated > peak) {
            peak = allocated;
        }

        // std::cout<<"Allocated " << curr_free
                //  << ", freelist head = " << freelist_head
                //  << ", allocated = " << allocated
                //  << ", peak = " << peak << std::endl;
        return reinterpret_cast<T*> (curr_free);
    }

    template<typename... Args>
    T* construct(Args&&... args) {
        T* slot = allocate();
        if (slot == nullptr) {
            return nullptr;
        }
        
        new (slot) T(std::forward<Args>(args)...);
        // std::cout<<"Constructed " << slot << std::endl;
        return slot;
    }

    void destroy(T* slot) {
        if (slot == nullptr) {
            // std::cout << "Cannot destroy nullptr" << std::endl;
            return;
        }
        slot->~T();
    }

    void deallocate(T* freedSlot) {
        if (freedSlot == nullptr) {
            // std::cout << "Cannot deallocate nullptr" << std::endl;
            return;
        }
        void* slot = reinterpret_cast<void*> (freedSlot);
        write_next(slot, freelist_head);
        freelist_head = slot;

        --allocated;

        // std::cout << "Deallocated " << slot
            //   << ", new freelist_head = " << freelist_head
            //   << ", allocated = " << allocated
            //   << ", peak = " << peak
            //   << std::endl;
    }

    void debug_state() const {
        std::cout << "=== Allocator State ===" << std::endl;
        std::cout << "freelist_head = " << freelist_head << std::endl;
        std::cout << "allocated = " << allocated << std::endl;
        std::cout << "peak = " << peak << std::endl;
    }

    void debug_freelist() const {
        std::cout << "=== Free List ===" << std::endl;

        void* curr = freelist_head;
        std::size_t index = 0;

        while (curr != nullptr) {
            std::cout << "Free node " << index
                    << ": current = " << curr
                    << ", next = " << *reinterpret_cast<void**>(curr)
                    << std::endl;
            curr = *reinterpret_cast<void**>(curr);
            ++index;
        }
    }
};
