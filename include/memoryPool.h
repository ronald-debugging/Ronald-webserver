#pragma once 

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>

namespace memoryPool
{
#define MEMORY_POOL_NUM 64
#define SLOT_BASE_SIZE 8
#define MAX_SLOT_SIZE 512


/* The slot size of a specific memory pool cannot be determined, because each memory pool has different slot sizes (multiples of 8)
So the sizeof of this slot structure is not the actual slot size */
struct Slot 
{
    Slot* next;
};

class MemoryPool
{
public:
    MemoryPool(size_t BlockSize = 4096);
    ~MemoryPool();
    
    void init(size_t);

    void* allocate();
    void deallocate(void*);
private:
    void allocateNewBlock();
    size_t padPointer(char* p, size_t align);

private:
    int        BlockSize_; // Memory block size
    int        SlotSize_; // Slot size
    Slot*      firstBlock_; // Points to the first actual memory block managed by the memory pool
    Slot*      curSlot_; // Points to the current unused slot
    Slot*      freeList_; // Points to free slots (slots that have been used and then released)
    Slot*      lastSlot_; // As a position identifier for the last element that can be stored in the current memory block
    std::mutex mutexForFreeList_; // Ensure atomicity of freeList_ in multi-threaded operations
    std::mutex mutexForBlock_; // Ensure unnecessary repeated memory allocation in multi-threaded situations
};


class HashBucket
{
public:
    static void initMemoryPool();
    static MemoryPool& getMemoryPool(int index);

    static void* useMemory(size_t size)
    {
        if (size <= 0)
            return nullptr;
        if (size > MAX_SLOT_SIZE) // For memory larger than 512 bytes, use new
            return operator new(size);

        // Equivalent to size / 8 rounded up (because allocated memory can only be larger, not smaller)
        return getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).allocate();
    }

    static void freeMemory(void* ptr, size_t size)
    {
        if (!ptr)
            return;
        if (size > MAX_SLOT_SIZE)
        {
            operator delete(ptr);
            return;
        }

        getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).deallocate(ptr);
    }

    template<typename T, typename... Args> 
    friend T* newElement(Args&&... args);
    
    template<typename T>
    friend void deleteElement(T* p);
};

template<typename T, typename... Args>
T* newElement(Args&&... args)
{
    T* p = nullptr;
    // Select appropriate memory pool to allocate memory based on element size
    if ((p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)))) != nullptr)
        // Construct object on allocated memory
        new(p) T(std::forward<Args>(args)...);

    return p;
}

template<typename T>
void deleteElement(T* p)
{
    // Object destruction
    if (p)
    {
        p->~T();
         // Memory recycling
        HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));
    }
}

} // namespace memoryPool