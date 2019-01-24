#ifndef SIMULATE_ARRAYLETS_WINDOWS
#define SIMULATE_ARRAYLETS_WINDOWS

#include <iostream>
#include <string.h>
#include <tchar.h>
#include <windows.h>

// To run: open Developer Command Prompt for VS 2017
// cl /EHsc simulateArrayLetsWindows.cpp
// simulateArrayLetsWindows

int main () {

    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    std::cout
        << "MapViewOfFile must use an offset which is a multiple of " 
        << systemInfo.dwAllocationGranularity << std::endl;
    // result is 65536

    // std::size_t regionCount = 1000;
    ULARGE_INTEGER heapSize;
    // heapSize.QuadPart = 0x100000000; //4gb
    heapSize.QuadPart = 0x40000000; // 1GB

    // Create the heap
    HANDLE heapHandle = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE | SEC_RESERVE, // | SEC_LARGE_PAGES
        heapSize.HighPart,
        heapSize.LowPart,
        NULL);
    if (heapHandle == NULL) {
        std::cout << "Failed to create heap mapping\n";
        exit(1);
    }
    std::cout << "heapHandle=" << heapHandle << std::endl;
    
    void *heapPointer = MapViewOfFile(
        heapHandle,
        FILE_MAP_WRITE, // read and write access
        0,
        0,
        heapSize.QuadPart);
    if (heapPointer == NULL) {
        std::cout << "Failed to commit heap\n";
        exit(1);
    }
    std::cout << "heapPointer=" << heapPointer << std::endl;

    // Commit 1 GB
    VirtualAlloc(
        heapPointer,
        0x40000000, // 1 gb
        MEM_COMMIT,
        PAGE_READWRITE);

    // Create the arraylet
    // fill in two regions of 64kb
    std::cout << "writing arraylet" << std::endl;
    memset(heapPointer, 'a', 65536);
    memset((char*)heapPointer+65536, 'b', 65536);

    // Create the arraylet
    void *arraylet = VirtualAlloc(
        NULL,
        65536*2,
        MEM_RESERVE,
        PAGE_NOACCESS);
    if (arraylet == NULL) {
        std::cout << "Failed to commit arraylet\n";
        exit(1);
    }
    std::cout << "arraylet=" << arraylet << std::endl;

    // Must free this address to map the file view
    if (VirtualFree(arraylet, 0, MEM_RELEASE) == 0) {
        std::cout << "Failed to free arraylet\n";
    }
    std::cout << "Free'd the region\n";
    
    // ULARGE_INTEGER arrayletBase;
    // arrayletBase.QuadPart = arraylet;

    for(int i = 0; i <= 1; i++) {
        void *arrayletPointer = MapViewOfFileEx(
            heapHandle,
            FILE_MAP_WRITE, // read and write access
            0,
            i*65536,
            65536,
            (char*)arraylet+i*65536);
        if (arrayletPointer == NULL) {
            std::cout << "Failed to map arraylet[" << i << "] to " <<(void*) ((char*)arraylet+i*65536) << std::endl;
            // exit(1);
        }
        std::cout << "arrayletPointer[" << i << "]=" << (void *)((char*)arraylet+i*65536) << std::endl;
    }

    char *arrayletPtr = (char *)arraylet;
    std::cout << arrayletPtr[0] << " " << arrayletPtr[65536] << std::endl;
    arrayletPtr[0] = 'c';
    arrayletPtr[65536]= 'd';

    char *heapPointer2 = (char *) heapPointer;
    std::cout << heapPointer2[0] << " " << heapPointer2[65536] << std::endl;
}

#endif /* SIMULATE_ARRAYLETS_WINDOWS */