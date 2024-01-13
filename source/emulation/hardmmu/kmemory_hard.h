#ifndef __KMEMORY_HARD_H__
#define __KMEMORY_HARD_H__

#ifdef BOXEDWINE_64BIT_MMU

#include "../cpu/binaryTranslation/btMemory.h"

class BtCodeChunk;

#define NATIVE_FLAG_COMMITTED 0x08
#define NATIVE_FLAG_CODEPAGE_READONLY 0x10

class KMemoryData : public BtMemory {
public:
    KMemoryData(KMemory* memory);
    ~KMemoryData();

    bool isAlignedNativePage(U32 page) { return (page & ~(K_NATIVE_PAGES_PER_PAGE - 1)) == page; }
    U32 getNativePage(U32 page) { return (page << K_PAGE_SHIFT) >> K_NATIVE_PAGE_SHIFT; }
    U32 getEmulatedPage(U32 nativePage) { return (nativePage << K_NATIVE_PAGE_SHIFT) >> K_PAGE_SHIFT; }

    // needed by kmemory
    bool isPageAllocated(U32 page);
    bool isPageMapped(U32 page);
    void allocPages(KThread* thread, U32 page, U32 pageCount, U8 permissions, FD fd, U64 offset, const BoxedPtr<MappedFile>& mappedFile, U8** ramPages = nullptr);
    void protectPage(KThread* thread, U32 i, U32 permissions);
    void setPagesInvalid(U32 page, U32 pageCount);
    void execvReset();
    void clearDelayedReset();
    U32 getPageFlags(U32 page);    
    bool reserveAddress(U32 startingPage, U32 pageCount, U32* result, bool canBeReMapped, bool alignNative, U32 reservedFlag);
    
    U8 flags[K_NUMBER_OF_PAGES];
    U8 nativeFlags[K_NATIVE_NUMBER_OF_PAGES]; // this is based on the granularity for permissions, Platform::getPagePermissionGranularity. 
    U32 allocated;
    U64 id;

    // this will contain id in each page unless that page was mapped to native host memory
    U64 memOffsets[K_NUMBER_OF_PAGES];
private:
    std::unordered_map<U32, std::unordered_map<U32, U32> > needsMemoryOffset; // first index is page, second index is offset
public:
    bool doesInstructionNeedMemoryOffset(U32 eip) {
        U32 page = eip >> K_PAGE_SHIFT;
        U32 offset = eip & K_PAGE_MASK;
        if (this->needsMemoryOffset.count(page) && this->needsMemoryOffset[page].count(offset)) {
            return this->needsMemoryOffset[page][offset] > 0;
        }
        return false;
    }
    void clearNeedsMemoryOffset(U32 page, U32 pageCount) {
        for (U32 i = 0; i < pageCount; i++) {
            if (this->needsMemoryOffset.count(page + i)) {
                this->needsMemoryOffset.erase(page + i);
            }
        }
    }
    void setNeedsMemoryOffset(U32 eip) {
        U32 page = eip >> K_PAGE_SHIFT;
        U32 offset = eip & K_PAGE_MASK;
        this->needsMemoryOffset[page][offset] = 1;
    }

    void clearAllNeedsMemoryOffset() {
        this->needsMemoryOffset.clear();
    }

#define MAX_DYNAMIC_CODE_PAGE_COUNT 0xFF
    U8 dynamicCodePageUpdateCount[K_NATIVE_NUMBER_OF_PAGES];
    
public:    
    void clearCodePageFromCache(U32 page);
    void clearHostCodeForWriting(U32 nativePage, U32 count);
    bool clearCodePageReadOnly(U32 nativePage);
    void makeCodePageReadOnly(U32 nativePage);
    void reserveNativeMemory();
    void releaseNativeMemory();    
    void* getNativeAddress(U32 address);
    
    void makeNativePageDynamic(U32 nativePage);
    

    void allocNativeMemory(U32 page, U32 pageCount, U32 flags);
    void freeNativeMemory(U32 page, U32 pageCount);
    void updatePagePermission(U32 page, U32 pageCount); // called after page permission has changed, code will give the native page the highest permission possible
    void updateNativePermission(U32 page, U32 pageCount, U32 permission); // for a native page change so that it can be read or written too now, updatePagePermission should be called when done to restore correct permissions

    bool isValidReadAddress(U32 address, U32 len);
    bool isValidWriteAddress(U32 address, U32 len);

    void unmapNativeMemory(U32 address, U32 size);
    U32 mapNativeMemory(void* hostAddress, U32 size);    

public:
    bool isShared(U32 page);           

private:
    U32 callbackPos;
    KMemory* memory;        
    KMemoryData* delayedReset;

    void addCallback(OpCallback func);
};

KMemoryData* getMemData(KMemory* memory);
#endif
#endif