#pragma once

#include "types.h"

namespace sead {

namespace RuntimeTypeInfo {
class Interface;
} // namespace RuntimeTypeInfo

class Heap {
public:
    virtual ~Heap();
    virtual const Heap* checkDerivedRuntimeTypeInfo(const RuntimeTypeInfo::Interface*) const;
    virtual const RuntimeTypeInfo::Interface* getRuntimeTypeInfo() const; 
    virtual void destroy();
    virtual size_t adjust();
    virtual void* tryAlloc(size_t size, s32 alignment);
    virtual void free(void *address);
    virtual size_t freeAndGetAllocatableSize(void *address, u32 alignment);
    virtual size_t resizeFront(void *address, size_t size);
    virtual size_t resizeBack(void *address, size_t size);
    virtual void* tryRealloc(void *address, size_t new_size, s32 alignment);
    virtual size_t freeAll();
    virtual void* getStartAddress() const;
    virtual void* getEndAddress() const;
    virtual size_t getSize() const;
    virtual size_t getFreeSize() const;
    virtual size_t getMaxAllocatableSize(int alignment) const;
    virtual bool isInclude(void *address) const;
    virtual bool isEmpty() const;
    virtual bool isFreeable() const;
    virtual bool isResizable() const;
    virtual bool isAdjustable() const;
};

} // namespace sead