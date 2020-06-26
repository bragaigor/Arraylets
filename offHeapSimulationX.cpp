#ifndef OFF_HEAP_SIMULATION
#define OFF_HEAP_SIMULATION

#include <iostream>
#include <fstream>
#include <string>

#define LINUX_ARRAYLET

#include "util.hpp"

// To run:
// For MAC
// g++ -g3 -Wno-write-strings -std=c++11 offHeapSimulationX.cpp -o offHeapSimulationX
// For Linux with no c++11 support
// g++ -g3 -Wno-write-strings -std=c++0x offHeapSimulationX.cpp -o offHeapSimulationX
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

	size_t pagesize = getpagesize(); // 4096 bytes
	std::cout << "System page size: " << pagesize << " bytes.\n";
	size_t arrayletSize = getArrayletSize(pagesize) * 4;
	std::cout << "Arraylet size: " << arrayletSize << " bytes" << std::endl;
	uintptr_t regionCount = 1024;
	uintptr_t offHeapRegionSize = FOUR_GB;
	uintptr_t offHeapSize = offHeapRegionSize * regionCount;
	uintptr_t inHeapSize = FOUR_GB;
	uintptr_t inHeapRegionSize = inHeapSize / regionCount;
	ElapsedTimer timer;
	timer.startTimer();

	int mmapProt = 0;
	int mmapFlags = 0;

	mmapProt = PROT_NONE; // PROT_READ | PROT_WRITE;
	mmapFlags = MAP_PRIVATE | MAP_ANON;

	int64_t elapsedTime1 = timer.getElapsedMicros();

	void *inHeapMmap = mmap(
                NULL,
                inHeapSize, // File size
                mmapProt,
                mmapFlags,
                -1, // File handle
                0);
	
	void *offHeapMmap = mmap(
                NULL,
                offHeapSize, // File size
                mmapProt,
                mmapFlags, 
                -1, // File handle
                0);

	int64_t elapsedTime2 = timer.getElapsedMicros();

	if (offHeapMmap == MAP_FAILED || inHeapMmap == MAP_FAILED) {
		std::cerr << "Failed to mmap " << strerror(errno) << "\n";
		return 1;
	} else {
		std::cout << "Successfully mmaped heap at address: " << (void *)offHeapMmap << "\n";
	}
	printf("In-heap address: %p, Off-heap address: %p\n", inHeapMmap, offHeapMmap);

	mmapProt = PROT_READ | PROT_WRITE;

	/* Setup in-heap with commited regions: */
	uintptr_t numOfComitRegions = 32;
	uintptr_t regionIndexes[numOfComitRegions] = {743, 34, 511, 2, 970, 888, 32, 100, 0, 123, 444, 3, 19, 721, 344, 471, 74, 234, 11, 20, 70, 188, 320, 150, 105, 203, 399, 32, 51, 727, 841, 47};
	void *leafAddresses[numOfComitRegions];
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

	char vals[SIXTEEN] = {'3', '5', '6', '8', '9', '0', '1', '2', '3', '7', 'A', 'E', 'C', 'B', 'D', 'F'};
	uintptr_t offsets[SIXTEEN] = {0, 12, 32, 45, 100, 103, 157, 198, 234, 281, 309, 375, 416, 671, 685, 949};
	size_t totalArraySize = 0;

	std::cout << "************************************************\n";

	/* Make sure we can read and write from this memory that we'll touch */
	int64_t elapsedTime3, elapsedTime4, elapsedTime5, elapsedTime6, elapsedTime7, elapsedTime8;

	/* Give hint to OS that we'll be using this memory range in the near future */
	void *offHeapChosenAddress = (void*)((uintptr_t)offHeapMmap + offsets[11] * offHeapRegionSize);
	intptr_t result2 = (intptr_t)madvise(offHeapChosenAddress, totalCalculatedComitedMem, MADV_WILLNEED);
	if (0 != result2) {
		printf("madvise returned -1 and errno: %d, error message: %s\n", errno, strerror(errno));
		return 1;
	}

	int result = 0;
	elapsedTime5 = timer.getElapsedMicros();
	/* Now we need to decommit memory from in-heap. Will madvise suffice? */
	for(int i = 0; i < numOfComitRegions; i++) {
		intptr_t ret = (intptr_t)madvise(leafAddresses[i], inHeapRegionSize, MADV_DONTNEED);
		//result = munmap(chosenAddress, inHeapRegionSize);
		//result = mprotect(chosenAddress, inHeapRegionSize, PROT_NONE);
		//void *result2 = mmap(chosenAddress, inHeapRegionSize, PROT_NONE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
		/*if (result2 == MAP_FAILED) {
			std::cerr << "Failed to mmap " << strerror(errno) << "\n";
			eturn 1;
		}*/
		if (0 != ret) {
			printf("madvise returned -1 and errno: %d, error message: %s\n", errno, strerror(errno));
			return 1;
		}
	}
	elapsedTime6 = timer.getElapsedMicros();
	
	printf("Just called madvise Sleeping for 5 seconds to fetch RSS. Should see 1GC RSS decrease!!!!!!\n");
	//sleep(5);

	double mmapTime = elapsedTime2 - elapsedTime1;
	double madviseTime = elapsedTime6 - elapsedTime5;

	// ###############################################

	/* Make sure we can read and write from this memory that we'll touch at off-heap */
	elapsedTime3 = timer.getElapsedMicros();
	mprotect(offHeapChosenAddress, totalCalculatedComitedMem, mmapProt);
	elapsedTime4 = timer.getElapsedMicros();

	/* Touch off-heap memory region */
	elapsedTime5 = timer.getElapsedMicros();
	memset(offHeapChosenAddress, vals[12], totalCalculatedComitedMem);
	elapsedTime6 = timer.getElapsedMicros();

	printf("Just called off-heap region memset... About to sleep for 5 seconds zZzZzZzZ\n");
	//sleep(5);

	elapsedTime7 = timer.getElapsedMicros();
	result = (intptr_t)madvise(offHeapChosenAddress, totalCalculatedComitedMem, MADV_DONTNEED);
	elapsedTime8 = timer.getElapsedMicros();
	if (0 != result) {
		printf("madvise returned -1 and errno: %d, error message: %s\n", errno, strerror(errno));
		return 1;
	} else {
		printf("madvise executed successfully!!\n");
	}

	// ###############################################

	double mprotectTime2 = elapsedTime4 - elapsedTime3;
        double memsetTime2 = elapsedTime6 - elapsedTime5;
        double madviseTime2 = elapsedTime8 - elapsedTime7;

	printf("Time taken to mmap %zu bytes: %.2f us, in seconds: %.2f\n", offHeapSize, mmapTime, (mmapTime/1000000));
	printf("Time taken to madvise %zu bytes: %.2f us, in seconds: %.2f\n\n", totalCalculatedComitedMem, madviseTime, (madviseTime/1000000));

	printf("Time taken to mprotect off_heap region %zu: %.2f us, in seconds: %.2f\n", totalCalculatedComitedMem, mprotectTime2, (mprotectTime2/1000000));
	printf("Time taken to memset %zu at off-heap with 2's: %.2f us, in seconds: %.2f\n", totalCalculatedComitedMem, memsetTime2, (memsetTime2/1000000));
	printf("Time taken to madvise %zu bytes DONTNEED at off-heap: %.2f us, in seconds: %.2f\n\n", totalCalculatedComitedMem, madviseTime2, (madviseTime2/1000000));

	munmap(inHeapMmap, inHeapSize);
	munmap(offHeapMmap, offHeapSize);
	printf("Sleeping for 5 seconds unmapping off-heap. Fetch RSS\n");
	//sleep(5);

    return 0;
}

#endif /* OFF_HEAP_SIMULATION */

