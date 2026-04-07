# Fixed-Size Pool Allocator — C++

---

## Table of Contents

1. [Overview & Motivation](#1-overview--motivation)
2. [Core Design](#2-core-design)
   - [Template Signature](#template-signature)
   - [Slot Sizing](#slot-sizing)
   - [Raw Storage](#raw-storage)
3. [Memory Model](#3-memory-model)
   - [Slot Layout](#slot-layout)
   - [write_next / read_next](#write_next--read_next)
   - [allocate() — Free List Pop](#allocate--free-list-pop)
   - [deallocate() — Free List Push](#deallocate--free-list-push)
   - [Placement New — construct()](#placement-new--construct)
   - [destroy()](#destroy)
4. [API Reference](#4-api-reference)
5. [Object Lifetime vs. Memory Lifetime](#5-object-lifetime-vs-memory-lifetime)
6. [Benchmarking](#6-benchmarking)
   - [Results](#results)
   - [Common Traps](#common-benchmark-traps)
   - [Recommended Pattern](#recommended-benchmark-pattern)
7. [Limitations & Future Work](#7-limitations--future-work)
8. [Repo Structure](#8-repo-structure)

---

## 1. Overview & Motivation

A fixed-size pool allocator pre-allocates one contiguous block of memory and divides it into equal-sized *slots*. Free slots are chained into a singly-linked free list. Allocation pops the head; deallocation pushes back.

**Use it when:**
- All objects are the same type
- Allocation/deallocation is on the hot path
- Heap fragmentation or latency spikes are unacceptable

> **Both allocation and deallocation are O(1)** — no heap metadata traversal, no size-class logic, no lock contention from the general allocator.

**Typical use-cases:** particle systems, ECS components, network packet objects, AST nodes, job queues, real-time systems.

---

## 2. Core Design

### Template Signature

```cpp
template<typename T, std::size_t N>
class PoolAllocator;
```

| Parameter | Meaning |
|-----------|---------|
| `T` | Type of object stored in the pool |
| `N` | Maximum number of live objects |

---

### Slot Sizing

```cpp
static constexpr std::size_t SLOT_SIZE =
    sizeof(T) > sizeof(void*) ? sizeof(T) : sizeof(void*);

static constexpr std::size_t ALIGNMENT =
    alignof(T) > alignof(void*) ? alignof(T) : alignof(void*);
```

Each slot must be large enough for two roles: hold a live `T` *or* store a `void*` free-list pointer when vacant.

```
SLOT_SIZE = max(sizeof(T), sizeof(void*))
ALIGNMENT = max(alignof(T), alignof(void*))
```

> **Example:** On a 64-bit system with `T = int` — `sizeof(int) = 4`, `sizeof(void*) = 8`, so `SLOT_SIZE = 8`. Each slot is 8 bytes even though an `int` needs only 4.

---

### Raw Storage

```cpp
alignas(ALIGNMENT) unsigned char pool[N * SLOT_SIZE];
```

`unsigned char` represents raw bytes — not an array of `T`. Objects only exist inside these bytes *after* placement new is called. `alignas(ALIGNMENT)` ensures every slot is correctly aligned for both roles.

---

## 3. Memory Model

### Slot Layout

Example: `N = 2`, `T = int`, 64-bit system → `SLOT_SIZE = 8`, total pool = 16 bytes.

```
Pool start = 0x1000

Slot 0:  0x1000 – 0x1007   (8 bytes)
Slot 1:  0x1008 – 0x100F   (8 bytes)

Initial free list:
  0x1000  →  stores pointer value  0x1008
  0x1008  →  stores nullptr
  freelist_head = 0x1000
```

A slot is not a single byte — it is a **range of bytes** starting at `pool + slot_index * SLOT_SIZE`. When you pass `slot_ptr(0)` to any cast, you are using `0x1000` as the start of an 8-byte region wide enough to hold a pointer.

---

### write_next / read_next

```cpp
void write_next(void* slot, void* next) {
    *reinterpret_cast<void**>(slot) = next;
}

void* read_next(void* slot) {
    return *reinterpret_cast<void**>(slot);
}
```

`reinterpret_cast<void**>(slot)` tells the compiler: *"the bytes at `slot` hold a `void*` value."* Dereferencing it reads or writes those 8 bytes as a pointer. The slot is guaranteed wide enough (`SLOT_SIZE`) to hold a pointer safely.

---

### allocate() — Free List Pop

```cpp
T* allocate() {
    if (!freelist_head) return nullptr;
    void* slot     = freelist_head;
    freelist_head  = read_next(slot);
    return reinterpret_cast<T*>(slot);
}
```

> **⚠ Important:** `reinterpret_cast<T*>(slot)` does **not** construct a `T`. It returns a typed pointer to raw bytes. You must call placement new before treating the pointer as a live object.

---

### deallocate() — Free List Push

```cpp
void deallocate(T* ptr) {
    void* slot = static_cast<void*>(ptr);
    write_next(slot, freelist_head);
    freelist_head = slot;
}
```

The slot's bytes are repurposed as free-list storage. Any object that was there must already be destroyed — reversing the order corrupts the object before its destructor runs.

---

### Placement New — construct()

```cpp
template<typename... Args>
T* construct(Args&&... args) {
    T* slot = allocate();
    if (!slot) return nullptr;
    new (slot) T(std::forward<Args>(args)...);
    return slot;
}
```

Normal `new` allocates memory **and** constructs. Placement new only constructs — inside already-owned memory. This is how allocators decouple memory lifetime from object lifetime.

---

### destroy()

```cpp
void destroy(T* ptr) {
    ptr->~T();
}
```

Calls the destructor explicitly. Required for non-trivial types (`std::string`, `std::vector`, classes with resources). For trivial types like `int` this is a no-op, but should still be called for correctness.

---

## 4. API Reference

### `T* allocate()`
Pops a free slot. Returns `nullptr` if the pool is exhausted. Does **not** construct any object. **O(1).**

---

### `void deallocate(T* ptr)`
Returns a slot to the free list. Does **not** call the destructor — call `destroy()` first. **O(1).**

---

### `T* construct(Args&&... args)`
Allocates a slot and constructs a `T` in it via placement new. Preferred over manual `allocate()` + placement new. Returns `nullptr` if the pool is full.

---

### `void destroy(T* ptr)`
Calls `ptr->~T()`. Must be called **before** `deallocate()`.

---

### Correct teardown order

```cpp
allocator.destroy(ptr);     // 1. end object lifetime
allocator.deallocate(ptr);  // 2. return memory to pool
```

> Reversing this order writes the free-list pointer into memory that the destructor hasn't finished using — undefined behaviour for any non-trivial type.

---

## 5. Object Lifetime vs. Memory Lifetime

This is the central systems concept the project teaches.

| Concept | Controlled by | What it means |
|---------|--------------|---------------|
| **Memory lifetime** | `allocate()` / `deallocate()` | Bytes are reserved. The pool owns them for its entire duration. |
| **Object lifetime** | `construct()` / `destroy()` | A constructor has run and a destructor has not yet run. Only in this window is the object valid to use. |

### Key mental models

**A slot has two roles.**
When free: stores a `void*` free-list pointer. When live: stores a `T`. Same bytes, different interpretation. This is why `reinterpret_cast` appears throughout.

**`reinterpret_cast` ≠ construction.**
Casting a pointer only changes how the compiler views an address. It does not call constructors. Placement new is what begins object lifetime.

**Placement new is the bridge.**
It turns raw bytes into a real C++ object without asking the heap for memory. `allocate()` provides the bytes; `new (ptr) T(...)` provides the object.

**`destroy()` and `deallocate()` are not the same.**
`destroy()` ends the object. `deallocate()` reclaims the memory. They must always be called in that order, and they must both be called.

---

## 6. Benchmarking

### Results

Bulk allocation benchmark — 1,000,000 objects, `T = int`, 64-bit Linux.

| Allocator | Time | Relative |
|-----------|------|---------|
| `new` / `delete` | 69,339 µs | 1× (baseline) |
| Pool allocator | 5,128 µs | **~13.5× faster** |

### Why the pool is faster

`new` / `delete` must handle: heap metadata management, size-class logic, fragmentation, thread-safety, coalescing, and complex reuse policies.

The pool does exactly two things:

```
allocate   →  pop one pointer off a stack
deallocate →  push one pointer onto a stack
```

### Common Benchmark Traps

| Trap | What goes wrong | Fix |
|------|----------------|-----|
| Compiler eliminates dead allocs | The loop is optimised away entirely — both allocators show 0 ms | Add `volatile int sink = 0` and read each object: `sink += *p` |
| Allocate + free in a tight loop | General allocator reuses the same hot block; caches stay warm | Bulk-allocate all N pointers first, then bulk-free |
| Too few objects | Not enough heap pressure to expose the real cost | Use 1,000,000+ objects |

### Recommended Benchmark Pattern

```cpp
constexpr int N = 1'000'000;
volatile int sink = 0;

// --- new/delete ---
auto t0 = now();
std::vector<int*> ptrs;
ptrs.reserve(N);
for (int i = 0; i < N; ++i)
    ptrs.push_back(new int(i));
for (auto* p : ptrs) {
    sink += *p;
    delete p;
}
auto t1 = now();

// --- pool ---
PoolAllocator<int, N> pool;
auto t2 = now();
std::vector<int*> pptrs;
pptrs.reserve(N);
for (int i = 0; i < N; ++i)
    pptrs.push_back(pool.construct(i));
for (auto* p : pptrs) {
    sink += *p;
    pool.destroy(p);
    pool.deallocate(p);
}
auto t3 = now();
```

The `sink` variable forces the compiler to keep every allocation and object access meaningful, preventing the entire loop from being optimised away.

---

## 7. Limitations & Future Work

| Limitation | Suggested improvement |
|------------|----------------------|
| Capacity `N` fixed at compile time | Add a fallback heap allocator when pool is exhausted |
| No double-free detection | Debug mode: maintain a bitmap of live slots |
| No `owns(T*)` check | Validate pointer falls within pool bounds before deallocating |
| Not thread-safe | Protect free-list head with a spinlock or atomic CAS |
| Single object type only | Generalise with type-erased slots for a slab-style allocator |
| No alignment validation on `deallocate` | Assert pointer is aligned to `SLOT_SIZE` offset from pool start |

---

## 8. Repo Structure

```
pool-allocator/
├── include/
│   └── pool_allocator.hpp   # header-only implementation
├── src/
│   ├── main.cpp             # usage examples
│   └── benchmark.cpp        # timing harness
├── CMakeLists.txt
└── README.md
```

---
