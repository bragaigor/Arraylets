#ifndef OFF_HEAP_SIMULATION
#define OFF_HEAP_SIMULATION

#include <iostream>
#include <fstream>
#include <errno.h>
//#include <sys/types.h>
#include <sys/mman.h>
#include <sys/shm.h>
//#include <sys/vminfo.h>
#include <string.h>
#include <unistd.h>

#include <ctime>
#include<time.h>

#define TWO_HUNDRED_56_MB 268435456
#define ONE_GB 1073741824 // 1GB
#define TWO_GB 2147483648 // 2GB
#define FOUR_GB 4294967296 // 4GB
#define EIGHT_GB 8589934592 // 8GB
#define SIXTEEN_GB 17179869184 // 16GB
#define SIXTY_FOUR_GB 68719476736 // 64GB

//#include "util.hpp"

// To run:
// /usr/vac/bin/xlc -g3 -std=c++0x offHeapSimulationAIX.cpp -o offHeapSimulationAIX
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

	uintptr_t pagesize = getpagesize(); // 4096 bytes
	std::cout << "System page size: " << pagesize << " bytes.\n";
	uintptr_t regionCount = 1024;
	uintptr_t offHeapRegionSize = (uintptr_t)FOUR_GB;
	uintptr_t offHeapSize = offHeapRegionSize * regionCount;
	uintptr_t inHeapSize = TWO_GB + (1024 * 1024 * 512);
	uintptr_t inHeapRegionSize = inHeapSize / regionCount;

	int mmapProt = 0;
	int mmapFlags = 0;

	mmapProt = PROT_NONE; // PROT_READ | PROT_WRITE;
	mmapFlags = MAP_PRIVATE | MAP_ANONYMOUS;

	printf("About to create inHeap with size: %zu, sizeof(uintptr_t): %zu\n", inHeapSize, sizeof(uintptr_t));

	clock_t elapsedTime1, elapsedTime2, elapsedTime3, elapsedTime4, elapsedTime5, elapsedTime6, elapsedTime7, elapsedTime8;

	void *inHeapMmap = mmap(
                NULL,
                inHeapSize,
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS,
                -1, // File handle
                0);
	
	void *offHeapMmap = mmap(
                NULL,
                offHeapSize, // File size
                mmapProt,
                mmapFlags, 
                -1, // File handle
                0);

	if (inHeapMmap == MAP_FAILED) {
                std::cerr << "Failed to mmap inheap " << strerror(errno) << "\n";
                return 1;
        } else {
                std::cout << "Successfully mmaped in-heap at address: " << (void *)offHeapMmap << "\n";
        }

	if (offHeapMmap == MAP_FAILED) {
		std::cerr << "Failed to mmap off-heap " << strerror(errno) << "\n";
		munmap(inHeapMmap, inHeapSize);
		return 1;
	} else {
		std::cout << "Successfully mmaped off-heap at address: " << (void *)offHeapMmap << "\n";
	}
	printf("In-heap address: %p, Off-heap address: %p\n", inHeapMmap, offHeapMmap);

	mmapProt = PROT_READ | PROT_WRITE;

	/* Setup in-heap with commited regions: */
#define NUM_COMMIT_REGIONS 32
	uintptr_t numOfComitRegions = NUM_COMMIT_REGIONS;
	uintptr_t regionIndexes[NUM_COMMIT_REGIONS] = {743, 34, 511, 2, 970, 888, 32, 100, 0, 123, 444, 3, 19, 721, 344, 471, 74, 234, 11, 20, 70, 188, 320, 150, 105, 203, 399, 32, 51, 727, 841, 47};
	void *leafAddresses[NUM_COMMIT_REGIONS];
	uintptr_t totalCalculatedComitedMem = 0;
	/* Populate in-heap regions */
	for(int i = 0; i < numOfComitRegions; i++) {
		void *chosenAddress = (void*)((uintptr_t)inHeapMmap + (regionIndexes[i] * inHeapRegionSize));
		leafAddresses[i] = chosenAddress;
		printf("\tPopulating address: %p with A's\n", chosenAddress);
		mprotect(chosenAddress, inHeapRegionSize, mmapProt);
		memset(chosenAddress, 'A', inHeapRegionSize);
		totalCalculatedComitedMem += inHeapRegionSize;
	}

	std::cout << "************************************************\n";
        char *someString = (char*)inHeapMmap;
        printf("Chars at: 0: %c, 100: %c, 500: %c, 1024: %c, 1048576: %c\n", *(someString + regionIndexes[0]*inHeapRegionSize), *(someString + regionIndexes[1]*inHeapRegionSize + 1024), *(someString + regionIndexes[2]*inHeapRegionSize + 10000), *(someString + regionIndexes[3]*inHeapRegionSize + 20000), *(someString + regionIndexes[6]*inHeapRegionSize + 50000));
	printf("######## Calculated commited memory: %zu bytes ##########\n", totalCalculatedComitedMem);

	printf("Sleeping for 15 seconds before we start doing anything. off-heap created successfully. Fetch RSS\n");
	//sleep(15);

#define SIXTEEN 16
	char vals[SIXTEEN] = {'3', '5', '6', '8', '9', '0', '1', '2', '3', '7', 'A', 'E', 'C', 'B', 'D', 'F'};
	uintptr_t offsets[SIXTEEN] = {0, 12, 32, 45, 100, 103, 157, 198, 234, 281, 309, 375, 416, 671, 685, 949};
	size_t totalArraySize = 0;

	std::cout << "************************************************\n";

	/* Give hint to OS that we'll be using this memory range in the near future */
	void *offHeapChosenAddress = (void*)((uintptr_t)offHeapMmap + offsets[11] * offHeapRegionSize);
	/* intptr_t result2 = (intptr_t)disclaim64(offHeapChosenAddress, (size_t)totalCalculatedComitedMem, DISCLAIM_ZEROMEM);

	if (0 != result2) {
		printf("disclaim64 returned -1 and errno: %d, error message: %s\n", errno, strerror(errno));
		return 1;
	} */

	int result = 0;
	/* Now we need to decommit memory from in-heap. Will disclaim64 suffice? */
	for(int i = 0; i < numOfComitRegions; i++) {
		intptr_t ret = (intptr_t)disclaim64(leafAddresses[i], (size_t)inHeapRegionSize, DISCLAIM_ZEROMEM);
		// intptr_t ret = (intptr_t)madvise(leafAddresses[i], inHeapRegionSize, MADV_DONTNEED);
		//result = munmap(chosenAddress, inHeapRegionSize);
		//result = mprotect(chosenAddress, inHeapRegionSize, PROT_NONE);
		//void *result2 = mmap(chosenAddress, inHeapRegionSize, PROT_NONE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
		/*if (result2 == MAP_FAILED) {
			std::cerr << "Failed to mmap " << strerror(errno) << "\n";
			eturn 1;
		}*/
		if (0 != ret) {
			printf("disclaim64 returned -1 and errno: %d, error message: %s\n", errno, strerror(errno));
			return 1;
		}
	}
	
	printf("Just called disclaim64 Sleeping for 5 seconds to fetch RSS. Should see 1GC RSS decrease!!!!!!\n");
	//sleep(5);

	double mmapTime = 0; //elapsedTime2 - elapsedTime1;
	double disclaim64Time = 0; //elapsedTime6 - elapsedTime5;

	// ###############################################

	/* Make sure we can read and write from this memory that we'll touch at off-heap */
	//elapsedTime3 = timer.getElapsedMicros();
	mprotect(offHeapChosenAddress, totalCalculatedComitedMem, mmapProt);
	//elapsedTime4 = timer.getElapsedMicros();

	/* Touch off-heap memory region */
	//elapsedTime5 = timer.getElapsedMicros();
	memset(offHeapChosenAddress, vals[12], totalCalculatedComitedMem);
	//elapsedTime6 = timer.getElapsedMicros();

	printf("Just called off-heap region memset... About to sleep for 5 seconds zZzZzZzZ\n");
	//sleep(5);

	//elapsedTime7 = timer.getElapsedMicros();
	result = (intptr_t)disclaim64(offHeapChosenAddress, (size_t)totalCalculatedComitedMem, DISCLAIM_ZEROMEM);
	//elapsedTime8 = timer.getElapsedMicros();
	if (0 != result) {
		printf("disclaim64 returned -1 and errno: %d, error message: %s\n", errno, strerror(errno));
		return 1;
	} else {
		printf("disclaim64 executed successfully!!\n");
	}

	// ###############################################

	double mprotectTime2 = 0; //elapsedTime4 - elapsedTime3;
        double memsetTime2 = 0; //elapsedTime6 - elapsedTime5;
        double disclaim64Time2 = 0; //elapsedTime8 - elapsedTime7;

	printf("Time taken to mmap %zu bytes: %.2f us, in seconds: %.2f\n", offHeapSize, mmapTime, (mmapTime/1000000));
	printf("Time taken to disclaim64 %zu bytes: %.2f us, in seconds: %.2f\n\n", totalCalculatedComitedMem, disclaim64Time, (disclaim64Time/1000000));

	printf("Time taken to mprotect off_heap region %zu: %.2f us, in seconds: %.2f\n", totalCalculatedComitedMem, mprotectTime2, (mprotectTime2/1000000));
	printf("Time taken to memset %zu at off-heap with 2's: %.2f us, in seconds: %.2f\n", totalCalculatedComitedMem, memsetTime2, (memsetTime2/1000000));
	printf("Time taken to disclaim64 %zu bytes DONTNEED at off-heap: %.2f us, in seconds: %.2f\n\n", totalCalculatedComitedMem, disclaim64Time2, (disclaim64Time2/1000000));

	munmap(inHeapMmap, inHeapSize);
	munmap(offHeapMmap, offHeapSize);
	printf("Sleeping for 5 seconds unmapping off-heap. Fetch RSS\n");
	//sleep(5);

    return 0;
}

#endif /* OFF_HEAP_SIMULATION */

