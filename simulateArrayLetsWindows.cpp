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
// simulateArrayLetsWindows 1432 100 0

void * mmapContiguous(size_t totalArraySize, size_t arrayletSize, long arrayLetOffsets[], HANDLE heapHandle, int debug, void* arrayletViews[]) 
   {
    // Creates contiguous memory space for arraylets
    void *contiguous = VirtualAlloc(
        NULL,           	// addr
        totalArraySize,		// size
        MEM_RESERVE /* | MEM_LARGE_PAGES */,    // type flags 
        PAGE_NOACCESS); 	// protection flags 
    if (contiguous == NULL) {
        std::cout << "Failed to commit contiguous block of memory\n";
        exit(1);
    }

    // MUST free this address to map the file view
    if (VirtualFree(contiguous, 0, MEM_RELEASE) == 0) {
        std::cout << "Failed to free contiguous\n";
    }

	for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
		void *nextAddress = (void *)((char*)((char*)contiguous+i*arrayletSize));
		arrayletViews[i] = MapViewOfFileEx(
            heapHandle,     	// file handle
            FILE_MAP_WRITE, 	// read and write access
            0,              	// fileHandle heap offset high
            arrayLetOffsets[i],	// fileHandle heap offset low
            arrayletSize,		// number of bytes to map 
            (char*)nextAddress);	// Offset into contiguous memory
			
		if (arrayletViews[i] == NULL) {
            std::cout << "Failed to map contiguous[" << i << "] to " <<(void*) ((char*)contiguous+i*arrayletSize) << std::endl;
            exit(1);
        } else if (arrayletViews[i] != nextAddress) {
			std::cout << "Map failed to provide the correct address. arrayletViews[" << i << "]: " << arrayletViews[i] << " nextAddress to " << nextAddress << std::endl;
            exit(1);
		}
		// std::cout << "Successfully mapped arrayletViews[" << i << "]=" << (void *)((char*)contiguous+i*arrayletSize) << std::endl;
	}
	
	if (1 == debug) {
		fprintf(stdout, "First 48 chars of data at contiguous block of memory BEFORE modification\n");
		for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
			char *arrayletDebug = (char*)contiguous+i*arrayletSize;
			fprintf(stdout, "\tcontiguous[%1lu] %.48s\n", i*arrayletSize, arrayletDebug);
		}
	}
	
    return contiguous;
   }

void freeArrayletViews(void* arrayletViews[])
   {
   for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
		if(UnmapViewOfFile(arrayletViews[i]) == 0){
			std::cout << "Failed to unmap View of File at arraylet leaf: " << i << ".\n";
			exit(1);
		}
	}
   }

int main (int argc, char** argv) {
	
	if (argc != 4) {
        std::cout<<"USAGE: " << argv[0] << " seed# iterations# debug<0,1>" << std::endl;
        std::cout << "Example: " << argv[0] << " 6363 100 0" << std::endl;
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
	size_t arrayletSize = getArrayletSize(pagesize); // Each arraylet will have 524288 bytes
    std::cout << "Arraylet size: " << arrayletSize << " bytes" << std::endl;

    ULARGE_INTEGER heapSize;
    heapSize.QuadPart = ONE_GB; //1gb
	uintptr_t heapSize2 = ONE_GB;

    // Create the heap
    HANDLE heapHandle = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE | SEC_RESERVE, // | SEC_COMMIT, // | SEC_LARGE_PAGES
        0, // heapSize.HighPart,
        heapSize2, // heapSize.LowPart,
        NULL);
    if (heapHandle == NULL) {
        std::cout << "Failed to create heap mapping\n";
        exit(1);
    }
    std::cout << "heapHandle=" << heapHandle << ", heapSize2: " << heapSize2 << ", heapSize.QuadPart: " << heapSize.QuadPart << 
				 ", heapSize.HighPart: " << heapSize.HighPart << ", heapSize.LowPart: " << heapSize.LowPart << std::endl;
    
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

	const size_t regionCount = 2048;
    void* regions[regionCount];
	size_t regionSize = 524288;
	
	for (size_t i = 0; i < regionCount; i++) {
		regions[i] = (void*)((char*)((char*)heapPointer+i*regionSize));
	}
		
	// Get page alligned offsets
    long arrayLetOffsets[ARRAYLET_COUNT];
    for(size_t i = 0; i < ARRAYLET_COUNT; i++) {
        // arrayLetOffsets[i] = getPageAlignedOffset(pagesize, rnd.nextNatural() % ONE_GB); // Change pagesize to match HUGETLB size
		arrayLetOffsets[i] = regionSize*(i*i+10);
		std::cout << "Arralylet at " << i << " has offset: " << arrayLetOffsets[i] << std::endl;
    }

    // Create the arraylet
    // Fill in arralet leaf regions
    std::cout << "Writing to arraylet leafs..." << std::endl;
	
	char vals[SIXTEEN] = {'3', '5', '6', '8', '9', '0', '1', '2', '3', '7', 'A', 'E', 'C', 'B', 'D', 'F'};
    size_t totalArraySize = 0;
	
	// Commit regions that will be used by arraylets 
	for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
		void *heapArraylet = (void*)((char*)heapPointer+arrayLetOffsets[i]);
		VirtualAlloc(
			heapArraylet,
			arrayletSize,
			MEM_COMMIT,
			PAGE_READWRITE);
		VirtualFree((LPVOID)heapArraylet, (size_t)0, MEM_RELEASE);
	}
	
	std::cout << "Just commited memory now..." << std::endl;
	
	for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
		void *heapArraylet = (void*)((char*)heapPointer+arrayLetOffsets[i]);
		VirtualAlloc(
			heapArraylet,
			arrayletSize,
			0,
			PAGE_READWRITE);
		memset((char*)heapPointer+arrayLetOffsets[i], vals[i%SIXTEEN], arrayletSize);
		//VirtualFree((LPVOID)heapArraylet, (size_t)0, MEM_RELEASE); // Why freeing it here everything else still works????
		totalArraySize += arrayletSize;
	}
	std::cout << "Writing to arraylet leafs complete." << std::endl;
	std::cout << "Total arraylet size: " << totalArraySize << std::endl;
	
	if (1 == debug) {
        fprintf(stdout, "First 48 chars of data BEFORE mapping and modification of the double mapped addresses\n");
        for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
            char *heapPtr = (char*)heapPointer+arrayLetOffsets[i];
            fprintf(stdout, "\theap[%1lu] %.48s\n", arrayLetOffsets[i], heapPtr);
        }
    }

	void* arrayletViews[ARRAYLET_COUNT];

	if(debug != 1) {

        double totalMapTime = 0;
        double totalModifyTime = 0;
        double totalFreeTime = 0;

		ElapsedTimer timer;
		timer.startTimer();
		
        double freeEnd = timer.getElapsedMicros();
		// LIMIT: 492 iterations. 
		for(size_t i = 0, j = 0; i < iterations; i++, j++) {
			// Make Arraylets look contiguous with VirtualAlloc
			void* contiguous = mmapContiguous(totalArraySize, arrayletSize, arrayLetOffsets, heapHandle, debug, arrayletViews);
			
            double mapEnd = timer.getElapsedMicros();

			// Modify contiguous memory view and observe change in the heap
			modifyContiguousMem(pagesize, arrayletSize, (char*)contiguous);

            double modifyEnd = timer.getElapsedMicros();

            freeArrayletViews(arrayletViews);
            
            totalMapTime += (mapEnd - freeEnd);
            
            freeEnd = timer.getElapsedMicros();
            
            totalModifyTime += (modifyEnd - mapEnd);
            totalFreeTime += (freeEnd - modifyEnd);
		}
		
		int64_t elapsedTime = timer.getElapsedMicros();
		double avgPerIter = (double)elapsedTime / iterations;
		std::cout << "Total Average iteration time: " << avgPerIter << " microseconds.\n";
        std::cout << "Test completed " << iterations << " iterations" << std::endl;
        std::cout << "Total elapsed time " << elapsedTime << "us (" << elapsedTime/1000000.0 << "s)" << std::endl;
        std::cout << "Total map time " << totalMapTime << "us (" << (totalMapTime/1000000) << "s) AVG map time " << (totalMapTime / iterations) << "us" << std::endl;
        std::cout << "Total modify time " << totalModifyTime << "us (" << (totalModifyTime/1000000) << "s) AVG modify time " << (totalModifyTime / iterations) << "us" << std::endl;
        std::cout << "Total free time " << totalFreeTime << "us (" << (totalFreeTime/1000000) << "s) AVG free time " << (totalFreeTime / iterations) << "us" << std::endl;

		
	} else {
		// Make Arraylets look contiguous with VirtualAlloc
		void* contiguous = mmapContiguous(totalArraySize, arrayletSize, arrayLetOffsets, heapHandle, debug, arrayletViews);
		
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

    if(UnmapViewOfFile(heapPointer) == 0){
        std::cout << "Failed to unmap heap pointer.\n";
        exit(1);
    }
    CloseHandle(heapHandle);
}

#endif /* SIMULATE_ARRAYLETS_WINDOWS */