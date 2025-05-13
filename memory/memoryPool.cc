#include "memoryPool.h"

namespace memoryPool 
{
MemoryPool::MemoryPool(size_t BlockSize)
    : BlockSize_ (BlockSize)
{}

MemoryPool::~MemoryPool()
{
    // Delete consecutive blocks
    Slot* cur = firstBlock_;
    while (cur)
    {
        Slot* next = cur->next;
        // Equivalent to free(reinterpret_cast<void*>(firstBlock_));
        // Convert to void pointer, because void type doesn't need to call destructor, only free space
        operator delete(reinterpret_cast<void*>(cur));
        cur = next;
    }
}

void MemoryPool::init(size_t size)
{
    assert(size > 0);
    SlotSize_ = size;
    firstBlock_ = nullptr;
    curSlot_ = nullptr;
    freeList_ = nullptr;
    lastSlot_ = nullptr;
}

void* MemoryPool::allocate()
{
    // Prioritize using memory slots from the free list
    if (freeList_ != nullptr)
    {
        {
            std::lock_guard<std::mutex> lock(mutexForFreeList_);
            if (freeList_ != nullptr)
            {
                Slot* temp = freeList_;
                freeList_ = freeList_->next;
                return temp;
            }
        }
    }

    Slot* temp;
    {   
        std::lock_guard<std::mutex> lock(mutexForBlock_);
        if (curSlot_ >= lastSlot_)
        {
            // Current memory block has no available slots, allocate a new memory block
            allocateNewBlock();
        }
    
        temp = curSlot_;
        // Cannot directly do curSlot_ += SlotSize_ here because curSlot_ is of type Slot*, so need to divide by SlotSize_ and add 1
        curSlot_ += SlotSize_ / sizeof(Slot);
    }
    
    return temp; 
}

void MemoryPool::deallocate(void* ptr)
{
    if (ptr)
    {
        // Recycle memory, insert memory into free list through head insertion
        std::lock_guard<std::mutex> lock(mutexForFreeList_);
        reinterpret_cast<Slot*>(ptr)->next = freeList_;
        freeList_ = reinterpret_cast<Slot*>(ptr);
    }
}

void MemoryPool::allocateNewBlock()
{   
    //std::cout << "Apply for a memory block, SlotSize: " << SlotSize_ << std::endl;
    // Insert new memory block using head insertion
    void* newBlock = operator new(BlockSize_);
    reinterpret_cast<Slot*>(newBlock)->next = firstBlock_;
    firstBlock_ = reinterpret_cast<Slot*>(newBlock);

    char* body = reinterpret_cast<char*>(newBlock) + sizeof(Slot*);
    size_t paddingSize = padPointer(body, SlotSize_); // Calculate padding size needed for alignment
    curSlot_ = reinterpret_cast<Slot*>(body + paddingSize);

    // If exceeding this mark position, it means the memory block has no available slots, need to request a new memory block from the system
    lastSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<size_t>(newBlock) + BlockSize_ - SlotSize_ + 1);

    freeList_ = nullptr;
}

// Align pointer to multiple of slot size
size_t MemoryPool::padPointer(char* p, size_t align)
{
    // align is the slot size
    return (align - reinterpret_cast<size_t>(p)) % align;
}

void HashBucket::initMemoryPool()
{
    for (int i = 0; i < MEMORY_POOL_NUM; i++)
    {
        getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
    }
}   

// Singleton pattern
MemoryPool& HashBucket::getMemoryPool(int index)
{
    static MemoryPool memoryPool[MEMORY_POOL_NUM];
    return memoryPool[index];
}

} // namespace memoryPool
