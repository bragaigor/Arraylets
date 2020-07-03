#ifndef OFF_HEAP_SIMULATION
#define OFF_HEAP_SIMULATION

#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <errno.h>
#include <ctime>
#include<time.h>


#define TWO_HUNDRED_56_MB 268435456
#define ONE_GB 1073741824 // 1GB
#define TWO_GB 2147483648 // 2GB
#define FOUR_GB 4294967296 // 4GB
#define EIGHT_GB 8589934592 // 8GB
#define SIXTEEN_GB 17179869184 // 16GB
#define SIXTY_FOUR_GB 68719476736 // 64GB

// To run:
// "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\x86_amd64"\vcvarsx86_amd64.bat
// "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\x86_amd64"\cl /EHsc offHeapSimulationWin.cpp
// For Linux with no c++11 support
// "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\x86_amd64"\cl /EHsc offHeapSimulationWin.cpp
// Note: Insert -lrt flag for linux systems
// ./offHeapSimulationX

#if !defined(MAP_HUGETLB)
#define MAP_HUGETLB 0x0040000
#endif

/**
 */

int main(int argc, char** argv) {

	/*if (argc != 4) {
		std::cout<<"USAGE: " << argv[0] << " seed# iterations# debug<0,1>" << std::endl;
		std::cout << "Example: " << argv[0] << " 6363 50000 0" << std::endl;
		return 1;
	}
    
	PaddedRandom rnd;
	int seed = atoi(argv[1]);
	int iterations = atoi(argv[2]);
	int debug = atoi(argv[3]);
	rnd.setSeed(seed);
	*/

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
	size_t pagesize = sysInfo.dwPageSize; // 4096 bytes
	std::cout << "System page size: " << pagesize << " bytes.\n";
	uintptr_t regionCount = 1024;
	uintptr_t offHeapRegionSize = FOUR_GB;
	uintptr_t offHeapSize = offHeapRegionSize * regionCount;
	uintptr_t inHeapSize = FOUR_GB;
	uintptr_t inHeapRegionSize = inHeapSize / regionCount;

    clock_t elapsedTime1, elapsedTime2, elapsedTime3, elapsedTime4, elapsedTime5, elapsedTime6, elapsedTime7, elapsedTime8;

	int mmapProt = 0;
	int mmapFlags = 0;

    mmapProt = PAGE_NOACCESS;
	mmapFlags = MEM_TOP_DOWN | MEM_RESERVE;

	elapsedTime1 = clock();

    void *inHeapMmap = VirtualAlloc(
                NULL, 
                (SIZE_T)inHeapSize, 
                mmapFlags, 
                mmapProt);
	
    void *offHeapMmap = VirtualAlloc(
                NULL, 
                (SIZE_T)offHeapSize, 
                mmapFlags, 
                mmapProt);

	elapsedTime2 = clock();

	if (offHeapMmap == NULL || inHeapMmap == NULL) {
		std::cerr << "Failed to mmap " << strerror(errno) << "\n";
		return 1;
	} else {
		std::cout << "Successfully mmaped heap at address: " << (void *)offHeapMmap << "\n";
	}
	printf("In-heap address: %p, Off-heap address: %p\n", inHeapMmap, offHeapMmap);

    printf("Sleeping for 15 seconds before we start doing anything. off-heap created successfully. Fetch RSS\n");
	Sleep(15000);

	int mmapProtNew = PAGE_READWRITE;

	/* Setup in-heap with commited regions: */
#define NUM_COMMIT_REGIONS 32
	uintptr_t regionIndexes[NUM_COMMIT_REGIONS] = {743, 34, 511, 2, 970, 888, 32, 100, 0, 123, 444, 3, 19, 721, 344, 471, 74, 234, 11, 20, 70, 188, 320, 150, 105, 203, 399, 32, 51, 727, 841, 47};
	void *leafAddresses[NUM_COMMIT_REGIONS];
	uintptr_t totalCalculatedComitedMem = 0;
	/* Populate in-heap regions */
	for(int i = 0; i < NUM_COMMIT_REGIONS; i++) {
		void *chosenAddress = (void*)((uintptr_t)inHeapMmap + (regionIndexes[i] * inHeapRegionSize));
		leafAddresses[i] = chosenAddress;
		printf("\tIter: %d, Populating address: %p with A's\n", i, chosenAddress);
        void *inHeapMmap2 = VirtualAlloc(
                (LPVOID)chosenAddress, 
                (SIZE_T)inHeapRegionSize, 
                MEM_TOP_DOWN | MEM_COMMIT, 
                PAGE_READWRITE);
            
        if (inHeapMmap2 == NULL) {
            printf("Failed to commit memory at: %p in in-heap...\n", chosenAddress);
        }
		memset(chosenAddress, 'A', inHeapRegionSize);
		totalCalculatedComitedMem += inHeapRegionSize;
	}

    std::cout << "************************************************\n";
    char *someString = (char*)inHeapMmap;
    printf("Chars at: 0: %c, 100: %c, 500: %c, 1024: %c, 1048576: %c\n", *(someString + regionIndexes[0]*inHeapRegionSize), *(someString + regionIndexes[1]*inHeapRegionSize + 1024), *(someString + regionIndexes[2]*inHeapRegionSize + 10000), *(someString + regionIndexes[3]*inHeapRegionSize + 20000), *(someString + regionIndexes[6]*inHeapRegionSize + 50000));
    printf("######## Calculated commited memory: %zu bytes ##########\n", totalCalculatedComitedMem);

	printf("Sleeping for 5 seconds after we commmitted in-heap memory\n");
	Sleep(5000);
#define SIXTEEN 16
	char vals[SIXTEEN] = {'3', '5', '6', '8', '9', '0', '1', '2', '3', '7', 'A', 'E', 'C', 'B', 'D', 'F'};
	uintptr_t offsets[SIXTEEN] = {0, 12, 32, 45, 100, 103, 157, 198, 234, 281, 309, 375, 416, 671, 685, 949};
	size_t totalArraySize = 0;

	std::cout << "************************************************\n";

	/* Make sure we can read and write from this memory that we'll touch */
	/* Give hint to OS that we'll be using this memory range in the near future */
	void *offHeapChosenAddress = (void*)((uintptr_t)offHeapMmap + offsets[11] * offHeapRegionSize);

	int result = 0;
	elapsedTime5 = clock();
	/* Now we need to decommit memory from in-heap. Will madvise suffice? */
	for(int i = 0; i < NUM_COMMIT_REGIONS; i++) {
        intptr_t ret = (intptr_t)VirtualFree((LPVOID)leafAddresses[i], (SIZE_T)inHeapRegionSize, MEM_DECOMMIT);
		if (0 == ret) {
			printf("VirtualFree returned 0 and errno: %d, error message: %s\n", errno, strerror(errno));
			return 1;
		}
	}
	elapsedTime6 = clock();
	
	printf("Just called madvise Sleeping for 5 seconds to fetch RSS. Should see 1GC RSS decrease!!!!!!\n");
	Sleep(5000);

	double mmapTime = (double)elapsedTime2 - (double)elapsedTime1;
	double virtualFreeTime = (double)elapsedTime6 - (double)elapsedTime5;

	// ###############################################

	/* Make sure we can read and write from this memory that we'll touch at off-heap */
	elapsedTime3 = clock();
    // VirtualProtect((LPVOID)offHeapChosenAddress, totalCalculatedComitedMem, (DWORD)mmapProtNew, (PDWORD)&mmapProt);
    void *offHeapMmap2 = VirtualAlloc(
                (LPVOID)offHeapChosenAddress, 
                (SIZE_T)totalCalculatedComitedMem, 
                MEM_TOP_DOWN | MEM_COMMIT, 
                PAGE_READWRITE);
	elapsedTime4 = clock();

    if (offHeapMmap2 == NULL) {
        printf("Failed to ");
    }

	/* Touch off-heap memory region */
	elapsedTime5 = clock();
	memset(offHeapChosenAddress, vals[12], totalCalculatedComitedMem);
	elapsedTime6 = clock();

	printf("Just called off-heap region memset... About to Sleep for 5 seconds zZzZzZzZ\n");
	Sleep(5000);

	elapsedTime7 = clock();
    result = (intptr_t)VirtualFree((LPVOID)offHeapChosenAddress, (SIZE_T)totalCalculatedComitedMem, MEM_DECOMMIT);
	elapsedTime8 = clock();
	if (0 == result) {
		printf("VirtualFree returned 0 and errno: %d, error message: %s\n", errno, strerror(errno));
		return 1;
	} else {
		printf("VirtualFree executed successfully!!\n");
	}

	// ###############################################

    double mprotectTime2 = (double)elapsedTime4 - (double)elapsedTime3;
    double memsetTime2 = (double)elapsedTime6 - (double)elapsedTime5;
    double virtualFreeTime2 = (double)elapsedTime8 - (double)elapsedTime7;

	printf("Time taken to mmap %zu bytes: %.2f us, in seconds: %.2f\n", offHeapSize, mmapTime, (mmapTime/CLOCKS_PER_SEC));
	printf("Time taken to madvise %zu bytes: %.2f us, in seconds: %.2f\n\n", totalCalculatedComitedMem, virtualFreeTime, (virtualFreeTime/CLOCKS_PER_SEC));

	printf("Time taken to mprotect off_heap region %zu: %.2f us, in seconds: %.2f\n", totalCalculatedComitedMem, mprotectTime2, (mprotectTime2/CLOCKS_PER_SEC));
	printf("Time taken to memset %zu at off-heap with 2's: %.2f us, in seconds: %.2f\n", totalCalculatedComitedMem, memsetTime2, (memsetTime2/CLOCKS_PER_SEC));
	printf("Time taken to VirtualFree %zu bytes MEM_DECOMMIT at off-heap: %.2f us, in seconds: %.2f\n\n", totalCalculatedComitedMem, virtualFreeTime2, (virtualFreeTime2/CLOCKS_PER_SEC));

    intptr_t ret1 = (intptr_t)VirtualFree((LPVOID)inHeapMmap, 0, MEM_RELEASE);
    intptr_t ret2 = (intptr_t)VirtualFree((LPVOID)offHeapMmap, 0, MEM_RELEASE);

    if (ret1 != 0 && ret2 != 0) {
        printf("Failed trying to free in-heap and/or off-heap: ret1: %ld, ret2: %ld\n", ret1, ret2);
    }

	printf("Sleeping for 5 seconds unmapping off-heap. Fetch RSS\n");
	Sleep(5000);

    return 0;
}

#endif /* OFF_HEAP_SIMULATION */

