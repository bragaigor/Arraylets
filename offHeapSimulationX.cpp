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

class OffHeapList {

	struct FreeList {
		void *lowAddress;
		void *highAddress;
		uintptr_t size;
		FreeList *next;
	};

public:

	/* Constructor */
	OffHeapList(void *startAddress, uintptr_t size) 
		: nodeCount(0),
		totalFreeSpace(size)
	{
		initFreeList(startAddress, size);
	}

	/* Destructor */
	~OffHeapList() {
		printf("Destructor called. Freeing all nodes in FreeList\n");
		freeAllList();
	}

	void initFreeList(void *startAddress, uintptr_t size) {
		head = createNewNode(startAddress, size);
		head->lowAddress = startAddress;
		head->highAddress = (void*)((uintptr_t)startAddress + size);
		head->size = size;
		head->next = NULL;
	}

	void *findAvailableAddress(uintptr_t size) {
		FreeList *previous = NULL;
		FreeList *current = head;
		void *returnAddr = NULL;

		while(NULL != current) {
			uintptr_t currSize = current->size;
			if(currSize >= size) {
				returnAddr = current->lowAddress;
				if(currSize == size) {
					/* Remove from FreeList since resulting size is 0 */
					if (NULL == previous) {
						head = current->next;
					} else {
						previous->next = current->next;
					}
					delete current;
					nodeCount--;
				} else {
					/* Update current entry */
					current->lowAddress = (void*)((uintptr_t)returnAddr + size);
					current->size -= size;
				}
				break;
			}
		
			previous = current;
			current = current->next;
		}

		if (NULL != returnAddr) {
			totalFreeSpace -= size;
		}

		return returnAddr;
	}

	bool addEntryToFreeList(void *startAddress, uintptr_t size) {

		FreeList *previous = NULL;
		FreeList *current = head;
		void *endAddress = (void*)((uintptr_t)startAddress + size);
		bool placed = false;

		while (NULL != current) {
			void *lowAddress = current->lowAddress;
			void *highAddress = current->highAddress;

			if (startAddress > highAddress) {
				previous = current;
				current = current->next;
				continue;
			}

			if(endAddress == lowAddress) {
				/* Newly released memory is right before current node */
				current->lowAddress = startAddress;
				current->size += size;
			} else if (startAddress == highAddress) {
				/* Newly released memory is right after current node */
				current->highAddress = endAddress;
				current->size += size;
				/* Check if we should merge next node */
				FreeList *nextNode = current->next;
				if (NULL != nextNode) {
					if(nextNode->lowAddress == endAddress) {
						current->highAddress = nextNode->highAddress;
						current->size += nextNode->size;
						current->next = nextNode->next;
						delete nextNode;
						nodeCount--;
					}
				}
			} else if (endAddress < lowAddress) {
				//printf("Inserting startAddress: %p, size: %zu, before low address: %p\n", startAddress, size, lowAddress);
				/* Create new node and insert in between */
				FreeList *node = createNewNode(startAddress, size);
				if (NULL != previous) {
					previous->next = node;
				} else {
					head = node;
				}
				node->next = current;
			} else {
				printf("Unreachable!!!!!\n");
				return false;
			}

			placed = true;
			break;
		}
		
		/* We must insert node right at the end of the list */
		if (!placed) {
			FreeList *node = createNewNode(startAddress, size);
			previous->next = node;
		}

		totalFreeSpace += size;
		return true;
	}

	bool isEmpty() {
		return NULL == head || NULL == head->lowAddress;
	}

	void printFreeListStatus() {
		FreeList *current = head;
		printf("---------------------------------------------------------------\n");
		printf("Number of free list nodes: %zu\n", nodeCount);
		printf("Is Free list empty: %d\n", (int)isEmpty());
		printf("Total free space: %zu\n", totalFreeSpace);
		printf("##############################\n");
		if (!isEmpty()) {
			while (NULL != current) {
				printNode(current);
				printf("##############################\n");
				current = current->next;
			}
		}
		printf("---------------------------------------------------------------\n");
	}

private:

	void printNode(FreeList *node) {
		printf("Low address: %p\n", node->lowAddress);
		printf("High address: %p\n", node->highAddress);
		printf("Free size: %zu\n", node->size);
	}

	FreeList *createNewNode(void *startAddress, uintptr_t size) {
		FreeList *node = new FreeList;
		node->lowAddress = startAddress;
		node->highAddress = (void*)((uintptr_t)startAddress + size);
		node->size = size;
		node->next = NULL;
		nodeCount++;
		return node;
	}

	void freeAllList() {
		FreeList *current = head;
		if (!isEmpty()) {
			 while (NULL != current) {
				FreeList *temp = current;
				current = current->next;
				delete temp;
			 }
		}
	}

	FreeList *head;
	uintptr_t nodeCount;
	uintptr_t totalFreeSpace;

};

void testOffHeapList() {

	OffHeapList offHeapList((void *)0x01000, 1024*1024);
	offHeapList.printFreeListStatus();
	void *addr0 = offHeapList.findAvailableAddress(64);
	void *addr1 = offHeapList.findAvailableAddress(32);
	void *addr2 = offHeapList.findAvailableAddress(256);
	void *addr3 = offHeapList.findAvailableAddress(512);
	void *addr4 = offHeapList.findAvailableAddress(128);
	void *addr5 = offHeapList.findAvailableAddress(64);
	void *addr6 = offHeapList.findAvailableAddress(128);
	offHeapList.printFreeListStatus();
	bool ret1 = offHeapList.addEntryToFreeList(addr1, 32);
	bool ret2 = offHeapList.addEntryToFreeList(addr3, 512);
	bool ret3 = offHeapList.addEntryToFreeList(addr5, 64);
	offHeapList.printFreeListStatus();
	bool ret4 = offHeapList.addEntryToFreeList(addr2, 256);
	offHeapList.printFreeListStatus();
	addr1 = offHeapList.findAvailableAddress(200);
	addr6 =	offHeapList.findAvailableAddress(7392);
	offHeapList.printFreeListStatus();
}

int main(int argc, char** argv) {

	//testOffHeapList();
	//return 1;

	size_t pagesize = getpagesize(); // 4096 bytes
	std::cout << "System page size: " << pagesize << " bytes.\n";
	size_t arrayletSize = getArrayletSize(pagesize) * 4;
	std::cout << "Arraylet size: " << arrayletSize << " bytes" << std::endl;
	uintptr_t regionCount = 1024;
	//uintptr_t offHeapRegionSize = FOUR_GB;
	uintptr_t inHeapSize = FOUR_GB;
	uintptr_t offHeapSize = inHeapSize * 4;
	uintptr_t inHeapRegionSize = inHeapSize / regionCount;
	uintptr_t offHeapRegionSize = inHeapRegionSize;
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

	OffHeapList offHeapList(offHeapMmap, offHeapSize);

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

	// TODO: Insert big loop here to simulate decommiting and commiting of in-heap, off-heap memory respectively. Make it random?

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

