#ifndef OFF_HEAP_SIMULATION
#define OFF_HEAP_SIMULATION

#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <unistd.h>
#include "util.hpp"

#include <time.h>

//#include "util.hpp"

// To run:
// g++ -g3 -std=c++11 offHeapSimulationMac.cpp -o offHeapSimulationMac
// ./offHeapSimulationMac

#if !defined(MAP_HUGETLB)
#define MAP_HUGETLB 0x0040000
#endif

/**
 */

int main(int argc, char **argv) {

	// while (true) {
	// 	int size = 1024*1024*128;
	// 	void *x = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1 ,0);
	// 	printf("Got memory: x = %p\n", x);
	// 	memset(x, 1, size);

	// 	sleep(1);
	// 	madvise(x, size, MADV_FREE_REUSABLE);
	// 	sleep(1);
	// }

	uintptr_t pagesize = getpagesize(); // 4096 bytes
	uintptr_t pagesize2 = sysconf(_SC_PAGESIZE);
	printf("System page size: %zu bytes, sysconf(_SC_PAGESIZE): %zu\n", pagesize, pagesize2);
	uintptr_t regionCount = 1024;
	uintptr_t offHeapRegionSize = (uintptr_t)FOUR_GB;
	uintptr_t offHeapSize = offHeapRegionSize * regionCount;
	uintptr_t inHeapSize = (uintptr_t)(FOUR_GB);
	uintptr_t inHeapRegionSize = inHeapSize / regionCount;

	int mmapProt = 0;
	int mmapFlags = 0;

	mmapProt = PROT_NONE;					 // PROT_READ | PROT_WRITE;
	mmapFlags = MAP_PRIVATE | MAP_ANONYMOUS; //MAP_PRIVATE | MAP_ANONYMOUS;

	printf("About to create inHeap with size: %zu, sizeof(uintptr_t): %zu\n", inHeapSize, sizeof(uintptr_t));

	/* Try to look for a large enough free region */
	void *previousAddr = NULL;
	void *inHeapMmap = NULL;
	void *inHeapMmapTest = mmap(
			NULL,
			inHeapSize,
			mmapProt,
			mmapFlags,
			-1, // File handle
			0);
	if (inHeapMmapTest == MAP_FAILED) {
		printf("Failed to mmap inheap string error: %s\n", strerror(errno));
		return 1;
	} else {
		previousAddr = inHeapMmapTest;
		inHeapMmap = inHeapMmapTest;
		munmap(inHeapMmapTest, inHeapSize);
	}

	uintptr_t inHeapSize2 = 0;
	for (int i = 0; i < regionCount; i++) {
		if (NULL != previousAddr) {
			mmapFlags |= MAP_FIXED;
		}

		void *regionInHeapMmap = mmap(
			previousAddr,
			inHeapRegionSize,
			mmapProt,
			mmapFlags,
			-1, // File handle
			0);

		if (regionInHeapMmap == MAP_FAILED) {
			printf("Failed to mmap inheap region at iter %d, string error: %s\n", i, strerror(errno));
			return 1;
		} else {
			printf("Successfully reserved heap region in iter: %d, previous: %p, at: %p with size: %zu\n", i, previousAddr, regionInHeapMmap, inHeapRegionSize);
		}

		// intptr_t resultInHeap = (intptr_t)madvise(regionInHeapMmap, inHeapRegionSize, MADV_FREE_REUSABLE);
		// if (0 != resultInHeap) {
		// 	printf("madvise returned -1 after mmap for in-heap and errno: %d, error message: %s\n", errno, strerror(errno));
		// 	return 1;
		// }

		if (NULL == previousAddr) {
			inHeapMmap = regionInHeapMmap;
		}

		inHeapSize2 += inHeapRegionSize;
		previousAddr = (void *)((uintptr_t)regionInHeapMmap + inHeapRegionSize);
	}

	if (inHeapSize2 != inHeapSize) {
		printf("inHeapSize: %zu, and inHeapSize2: %zu should have the same value!!!\n", inHeapSize, inHeapSize2);
		return 1;
	}

	/* Remove MAP_FIXED from flag */
	mmapFlags &= ~MAP_FIXED;

	void *offHeapMmap = mmap(
		NULL,
		offHeapSize, // File size
		mmapProt,
		mmapFlags,
		-1, // File handle
		0);

	if (inHeapMmap == MAP_FAILED) {
		printf("Failed to mmap inheap %s\n", strerror(errno));
		return 1;
	} else {
		printf("Successfully mmaped in-heap at address: %p\n", (void *)inHeapMmap);
	}

	// intptr_t resultOffHeap = (intptr_t)madvise(offHeapMmap, inHeapRegionSize, MADV_FREE_REUSABLE);
	// if (0 != resultOffHeap) {
	// 	printf("madvise returned -1 after mmap for off-heap and errno: %d, error message: %s\n", errno, strerror(errno));
	// 	return 1;
	// }

	if (offHeapMmap == MAP_FAILED) {
		printf("Failed to mmap off-heap %s\n", strerror(errno));
		munmap(inHeapMmap, inHeapSize);
		return 1;
	} else {
		printf("Successfully mmaped off-heap at address: %p\n", (void *)offHeapMmap);
	}
	printf("In-heap address: %p, Off-heap address: %p\n", inHeapMmap, offHeapMmap);

	/*
	for (int i = 0; i < regionCount; i++) {
                void *chosenAddr = (void*)((uintptr_t)inHeapMmap + (i * inHeapRegionSize));
		mprotect(chosenAddr, inHeapRegionSize, PROT_READ | PROT_WRITE);
                memset(chosenAddr, 'A', inHeapRegionSize);
        }

	printf("Sleeping for 5 seconds before decommiting inHeapSize: %zu bytes\n", inHeapSize);
        slep(5);
        munmap(inHeapMmap, inHeapSize);
	munmap(offHeapMmap, offHeapSize);
        return 1;
	*/

	mmapProt = PROT_READ | PROT_WRITE;

	printf("Sleeping for 15 seconds before doing anything\n");
	sleep(15);

	/* Setup in-heap with commited regions: */
#define NUM_COMMIT_REGIONS 64
	uintptr_t numOfComitRegions = NUM_COMMIT_REGIONS;
	uintptr_t regionIndexes[NUM_COMMIT_REGIONS] = {743, 34, 511, 2, 970, 888, 32, 100, 0, 123, 444, 3, 19, 721, 344, 471, 74, 234, 11, 20, 70, 188, 320, 150, 105, 203, 399, 32, 51, 727, 841, 47, 4, 35, 512, 7, 971, 889, 12, 101, 8, 124, 454, 9, 18, 722, 346, 571, 94, 214, 12, 50, 60, 187, 318, 151, 505, 103, 398, 37, 52, 738, 822, 48};
	void *leafAddresses[NUM_COMMIT_REGIONS];
	uintptr_t totalCalculatedComitedMem = 0;
	/* Populate in-heap regions */
	for (int i = 0; i < numOfComitRegions; i++) {
		void *chosenAddress = (void *)((uintptr_t)inHeapMmap + (regionIndexes[i] * inHeapRegionSize));
		leafAddresses[i] = chosenAddress;
		printf("\tPopulating address: %p  with size: %zu with A's\n", chosenAddress, inHeapRegionSize);
		mprotect(chosenAddress, inHeapRegionSize, mmapProt);
		memset(chosenAddress, 'A', inHeapRegionSize);
		intptr_t result2 = (intptr_t)madvise(chosenAddress, inHeapRegionSize, MADV_FREE_REUSABLE);
		if (0 != result2) {
			printf("madvise returned -1 after memset and errno: %d, error message: %s\n", errno, strerror(errno));
			return 1;
		}
		totalCalculatedComitedMem += inHeapRegionSize;
	}

	printf("************************************************\n");
	char *someString = (char *)inHeapMmap;
	printf("Chars at: 0: %c, 100: %c, 500: %c, 1024: %c, 1048576: %c\n", *(someString + regionIndexes[0] * inHeapRegionSize), *(someString + regionIndexes[1] * inHeapRegionSize + 1024), *(someString + regionIndexes[2] * inHeapRegionSize + 10000), *(someString + regionIndexes[3] * inHeapRegionSize + 20000), *(someString + regionIndexes[6] * inHeapRegionSize + 50000));
	printf("######## Calculated commited memory: %zu bytes ##########\n", totalCalculatedComitedMem);

	printf("Sleeping for 5 seconds before we start doing anything. off-heap created successfully. Fetch RSS\n");
	sleep(15);

#define SIXTEEN 16
	char vals[SIXTEEN] = {'3', '5', '6', '8', '9', '0', '1', '2', '3', '7', 'A', 'E', 'C', 'B', 'D', 'F'};
	uintptr_t offsets[SIXTEEN] = {0, 12, 32, 45, 100, 103, 157, 198, 234, 281, 309, 375, 416, 671, 685, 949};
	size_t totalArraySize = 0;

	printf("************************************************\n");
	/* Give hint to OS that we'll be using this memory range in the near future */
	void *offHeapChosenAddress = (void *)((uintptr_t)offHeapMmap + offsets[11] * offHeapRegionSize);
	// mprotect(offHeapChosenAddress, totalCalculatedComitedMem, PROT_NONE);
	// intptr_t result2 = (intptr_t)madvise(offHeapChosenAddress, totalCalculatedComitedMem, MADV_DONTNEED);
	// result2 = (intptr_t)madvise(offHeapChosenAddress, totalCalculatedComitedMem, MADV_FREE_REUSE);
	// if (0 != result2) {
	// 	printf("madvise returned -1 and errno: %d, error message: %s\n", errno, strerror(errno));
	// 	return 1;
	// }

	/* Now we need to decommit memory from in-heap. Will madvise suffice? */
	for (int i = 0; i < numOfComitRegions; i++) {
		printf("Freeing address: %p with size: %zu\n", leafAddresses[i], inHeapRegionSize);
		intptr_t ret = 0;
		// ret = (intptr_t)madvise(leafAddresses[i], inHeapRegionSize, MADV_FREE);
		// // result2 = (intptr_t)madvise(leafAddresses[i], inHeapRegionSize, MADV_FREE_REUSE);
		// if (0 != ret) {
		// 	printf("ret: %zu in iter: %d and errno: %d, error message: %s\n", ret, i, errno, strerror(errno));
		// 	return 1;
		// }
	}
	printf("Just called madvise Sleeping for 5 seconds to fetch RSS. Should see 1GC RSS decrease!!!!!!\n");
	sleep(15);

	// ###############################################

	/* Make sure we can read and write from this memory that we'll touch at off-heap */
	mprotect(offHeapChosenAddress, totalCalculatedComitedMem, mmapProt);

	/* Touch off-heap memory region */
	// TODO: Measure time taken of memset
	memset(offHeapChosenAddress, vals[12], totalCalculatedComitedMem);
	intptr_t result2 = (intptr_t)madvise(offHeapChosenAddress, totalCalculatedComitedMem, MADV_FREE_REUSABLE);
	if (0 != result2) {
		printf("madvise returned -1 after memset at off-heap and errno: %d, error message: %s\n", errno, strerror(errno));
		return 1;
	}

	printf("Just called off-heap region memset... About to sleep for 5 seconds zZzZzZzZ\n");
	sleep(15);

	// TODO: measure time of madvise
	// intptr_t result = (intptr_t)madvise(offHeapChosenAddress, totalCalculatedComitedMem, MADV_FREE);

	// if (0 != result) {
	// 	printf("madvise returned -1 and errno: %d, error message: %s\n", errno, strerror(errno));
	// 	return 1;
	// } else {
	// 	printf("madvise executed successfully!! Sleeping for 5 again\n");
	// 	sleep(5);
	// }

	printf("Just called msync at off-heap, sleep for 5..\n");
	sleep(5);

	// ###############################################

	munmap(inHeapMmap, inHeapSize);
	munmap(offHeapMmap, offHeapSize);
	printf("Sleeping for 5 seconds unmapping off-heap. Fetch RSS\n");
	sleep(5);

	return 0;
}

#endif /* OFF_HEAP_SIMULATION */
