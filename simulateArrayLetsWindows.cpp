#ifndef SIMULATE_ARRAYLETS_WINDOWS
#define SIMULATE_ARRAYLETS_WINDOWS

#include <iostream>
#include <string.h>
#include <tchar.h>
#include <windows.h>
#include <inttypes.h>

#define WINDOWS_ARRAYLET

#include "util.hpp"

// To run: open Developer Command Prompt for VS 2017
// cl /EHsc simulateArrayLetsWindows.cpp
// simulateArrayLetsWindows

void * mmapContiguous(size_t totalArraySize, size_t arrayletSize, long arrayLetOffsets[], HANDLE heapHandle, int debug) 
   {
    // Creates contiguous memory space for arraylets
    void *arraylet = VirtualAlloc(
        NULL,           	// addr
        totalArraySize,		// size
        MEM_RESERVE /* | MEM_LARGE_PAGES */,    // type flags 
        PAGE_NOACCESS); 	// protection flags 
    if (arraylet == NULL) {
        std::cout << "Failed to commit contiguous block of memory\n";
        exit(1);
    }
    // std::cout << "arraylet=" << arraylet << std::endl;

    // MUST free this address to map the file view
    if (VirtualFree(arraylet, 0, MEM_RELEASE) == 0) {
        std::cout << "Failed to free arraylet\n";
    }
    // std::cout << "Free'd the region\n";
	
	for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
		void *arrayletPointer = MapViewOfFileEx(
            heapHandle,     	// file handle
            FILE_MAP_WRITE, 	// read and write access
            0,              	// fileHandle heap offset high
            arrayLetOffsets[i],	// fileHandle heap offset low
            arrayletSize,		// number of bytes to map 
            (char*)arraylet+i*arrayletSize);	// Offset into contiguous memory
			
		if (arrayletPointer == NULL) {
            std::cout << "Failed to map arraylet[" << i << "] to " <<(void*) ((char*)arraylet+i*arrayletSize) << std::endl;
            // exit(1);
        }
		// std::cout << "Successfully mapped arrayletPointer[" << i << "]=" << (void *)((char*)arraylet+i*arrayletSize) << std::endl;
	}
	
	if (1 == debug) {
		fprintf(stdout, "First 48 chars of data at contiguous block of memory BEFORE modification\n");
		for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
			char *arrayletDebug = (char*)arraylet+i*arrayletSize;
			fprintf(stdout, "\tcontiguous[%1lu] %.48s\n", i*arrayletSize, arrayletDebug);
		}
	}
	
    return arraylet;
   }

int main (int argc, char** argv) {
	
	if (argc != 4) {
        std::cout<<"USAGE: " << argv[0] << " seed# iterations# debug<0,1>" << std::endl;
        std::cout << "Example: " << argv[0] << " 6363 0" << std::endl;
        return 1;
    }
	
	PaddedRandom rnd;
    int seed = atoi(argv[1]);
	int iterations = atoi(argv[2]);
	int debug = atoi(argv[3]);
	rnd.setSeed(seed);

    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
	size_t pagesize = systemInfo.dwAllocationGranularity;
    std::cout
        << "MapViewOfFile must use an offset which is a multiple of " 
        << pagesize << std::endl;
    // result is 65536
	size_t arrayletSize = getArrayletSize(pagesize);
    std::cout << "Arraylet size: " << arrayletSize << " bytes" << std::endl;

    ULARGE_INTEGER heapSize;
    heapSize.QuadPart = ONE_GB; //1gb

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
    
    // Similar to mmap on POSIX
    void *heapPointer = MapViewOfFile(
        heapHandle,
        FILE_MAP_WRITE, // read and write access
        0,
        0,
        heapSize.QuadPart);
    if (heapPointer == NULL) {
        std::cout << "Failed to commit heap\n";
		std::cout << "Failed to commit heap. Error: " << GetLastError() << "\n";
        exit(1);
    }
    std::cout << "heapPointer=" << heapPointer << std::endl;

    // Commit 1 GB
    VirtualAlloc(
        heapPointer,
        ONE_GB, // 1 GB 
        MEM_COMMIT,
        PAGE_READWRITE);
		
	// Get page alligned offsets
    long arrayLetOffsets[ARRAYLET_COUNT];
    for(size_t i = 0; i < ARRAYLET_COUNT; i++) {
        arrayLetOffsets[i] = getPageAlignedOffset(pagesize, rnd.nextNatural() % ONE_GB); // Change pagesize to match HUGETLB size
		std::cout << "Arralylet at " << i << " has offset: " << arrayLetOffsets[i] << std::endl;
    }

    // Create the arraylet
    // Fill in arralet leaf regions
    std::cout << "Writing to arraylet leafs..." << std::endl;
	
	char vals[SIXTEEN] = {'3', '5', '6', '8', '9', '0', '1', '2', '3', '7', 'A', 'E', 'C', 'B', 'D', 'F'};
    size_t totalArraySize = 0;
	
	for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
		memset((char*)heapPointer+arrayLetOffsets[i], vals[i%SIXTEEN], arrayletSize);
		totalArraySize += arrayletSize;
	}
	std::cout << "Writing to arraylet leafs complete." << std::endl;
	
	if (1 == debug) {
        fprintf(stdout, "First 48 chars of data BEFORE mapping and modification of the double mapped addresses\n");
        for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
            char *heapPtr = (char*)heapPointer+arrayLetOffsets[i];
            fprintf(stdout, "\theap[%1lu] %.48s\n", arrayLetOffsets[i], heapPtr);
        }
    }
	
	if(debug != 1) {
		
		ElapsedTimer timer;
		timer.startTimer();
		
		// LIMIT: 492 iterations. 
		for(size_t i = 0; i < iterations; i++) {
			// Make Arraylets look contiguous with VirtualAlloc
			void* contiguous = mmapContiguous(totalArraySize, arrayletSize, arrayLetOffsets, heapHandle, debug);
			
			// Modify contiguous memory view and observe change in the heap
			modifyContiguousMem(pagesize, arrayletSize, (char*)contiguous);
		}
		
		int64_t elapsedTime = timer.getElapsedMicros();
		double avgPerIter = (double)elapsedTime / iterations;
		fprintf(stdout, "Total time spent to create and modify both contiguous and heap locations: %" PRId64 " microseconds (%.4f seconds)\n",
					elapsedTime, elapsedTime/1000000.0);
		fprintf(stdout, "Total Average iteration time: %.3f microseconds.\n", avgPerIter);
		
	} else {
		// Make Arraylets look contiguous with VirtualAlloc
		void* contiguous = mmapContiguous(totalArraySize, arrayletSize, arrayLetOffsets, heapHandle, debug);
		
		// Modify contiguous memory view and observe change in the heap
		modifyContiguousMem(pagesize, arrayletSize, (char*)contiguous);
		
		fprintf(stdout, "\n\t%48s\n\t%48s\n\t%48s\n\n", 
						"*******************************************************",
						"******** THE NEXT 2 OUTPUTS SHOULD MATCH! *************",
						"*******************************************************");
		fprintf(stdout, "First 48 chars of data at contiguous block of memory (2 first pages of each arraylet) AFTER modification:\n");
		for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
			char *arrayletDebug = (char*)contiguous+i*arrayletSize;
			char *arrayletDebug2 = (char*)contiguous+i*arrayletSize+pagesize;
			fprintf(stdout, "\tcontiguous[%1lu] %.48s\n", i*arrayletSize, arrayletDebug);
			fprintf(stdout, "\tcontiguous[%1lu] %.48s\n", i*arrayletSize+pagesize, arrayletDebug2);
		}
		std::cout << std::endl;
		fprintf(stdout, "First 48 chars of data AFTER mapping and modification of the double mapped addresses:\n");
		for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
			char *heapPtr = (char*)heapPointer+arrayLetOffsets[i];
			char *heapPtr2 = (char*)heapPointer+arrayLetOffsets[i]+pagesize;
			fprintf(stdout, "\theap[%1lu] %.48s\n", arrayLetOffsets[i], heapPtr);
			fprintf(stdout, "\theap[%1lu] %.48s\n", arrayLetOffsets[i]+pagesize, heapPtr2);
		}
	}

    CloseHandle(heapHandle);
}

#endif /* SIMULATE_ARRAYLETS_WINDOWS */