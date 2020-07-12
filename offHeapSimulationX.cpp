#ifndef OFF_HEAP_SIMULATION
#define OFF_HEAP_SIMULATION

#include <string>

#define LINUX_ARRAYLET

#include "util.hpp"
#include "offHeapSimulationX.hpp"

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

class OffHeapObjectList {

	struct ObjectAddrNode {
		void *address;
		uintptr_t size;
		ObjectAddrNode *next;
	};

public:

	OffHeapObjectList() : 
		nodeCount(0),
       		objTotalSize(0) {
	
	}

	~OffHeapObjectList() {
		freeAllList();
	}

	bool addObjToList(void *address, uintptr_t size) {
		ObjectAddrNode *previous = NULL;
		ObjectAddrNode *current = head;
		ObjectAddrNode *newNode = createNewNode(address, size);
		if (NULL == current) {
			head = newNode;
		} else {
			while (NULL != current && (current->address < address)) {
				previous = current;
				current = current->next;
			}

			newNode->next = current;
			if (NULL == previous) {
				head = newNode;
			} else {
				previous->next = newNode;
			}
		}
		objTotalSize += size;
		return true;
	}

	/**
	 * Removed half of used off-heap nodes and frees the memory associated to them with madvise
	 * @param OffHeapList *offHeapList	off-heap free list
	 *
	 * @return total bytes amount of freed memry
	 */
	uintptr_t removeHalfOfNodes(OffHeapList *offHeapList) {
		if (NULL == head || (nodeCount < 3)) {
			printf("List is empty or has less than 3 nodes. Nothing to do.\n");
			return 0;
		}

		ObjectAddrNode *current = head;
		uintptr_t nodesDeleted = 0;
		uintptr_t totalSizeFreed = 0;

		/* Delete every other node */
		while ((NULL != current) && (NULL != current->next)) {
			ObjectAddrNode *tempNode = current->next;
			current->next = current->next->next;
			void *startAddress = tempNode->address;
			uintptr_t objSize = tempNode->size;
			delete tempNode;
			offHeapList->addEntryToFreeList(startAddress, objSize);
			intptr_t ret = (intptr_t)madvise(startAddress, objSize, MADV_DONTNEED);
			if (0 != ret) {
				printf("madvise returned -1 trying to free off-heap region and errno: %d, error message: %s\n", errno, strerror(errno));
				return UINTMAX_MAX;
			}
			nodeCount--;
			nodesDeleted++;
			objTotalSize -= objSize;
			totalSizeFreed += objSize;
			current = current->next;
		}

		printf("Number of objects deleted: %zu, Total size freed: %zu\n", nodesDeleted, totalSizeFreed);
		return totalSizeFreed;
	}

	void printOffHeapObjectStatus() {
		printf("---------------------------------------------------------------\n");
		printf("Total number of objects: %zu\n", nodeCount);
		printf("Objects total size: %zu\n", objTotalSize);
		if (!isEmpty()) {
			printf("###################################\n");
			ObjectAddrNode *current = head;
			while (NULL != current) {
				printfObject(current);
				current = current->next;
			}
		}
		printf("---------------------------------------------------------------\n");
	}

	bool isEmpty() {
 		return NULL == head;
	}

private:
	ObjectAddrNode *createNewNode(void *address, uintptr_t size) {
		ObjectAddrNode *node = new ObjectAddrNode;
		node->address = address;
		node->size = size;
		nodeCount++;
		return node;
	}

	void printfObject(ObjectAddrNode *node) {
		printf("Object address at offheap: %p\n", node->address);
		printf("Object size: %zu\n", node->size);
	}

	void freeAllList() {
		ObjectAddrNode *current = head;
		if (!isEmpty()) {
			while (NULL != current) {
				ObjectAddrNode *temp = current;
				current = current->next;
				delete temp;
			}
		}
	}

	uintptr_t nodeCount;
	uintptr_t objTotalSize;
	ObjectAddrNode *head;
};

void OffHeapList::initFreeList(void *startAddress, uintptr_t size) {
	head = createNewNode(startAddress, size);
	biggestFreeSize = size;
	biggestFreeSizeAddr = startAddress;
	head->lowAddress = startAddress;
	head->highAddress = (void*)((uintptr_t)startAddress + size);
	head->size = size;
	head->next = NULL;
}

void *OffHeapList::findAvailableAddress(uintptr_t size) {
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
				if (biggestFreeSizeAddr == returnAddr) {
					biggestFreeSize -= size;
					biggestFreeSizeAddr = current->lowAddress;
				}
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

bool OffHeapList::addEntryToFreeList(void *startAddress, uintptr_t size) {

	FreeList *previous = NULL;
	FreeList *current = head;
	void *endAddress = (void*)((uintptr_t)startAddress + size);
	bool placed = false;

	while (NULL != current) {
		void *lowAddress = current->lowAddress;
		void *highAddress = current->highAddress;

		/* Lazy update */
		if (current->size > biggestFreeSize) {
			biggestFreeSize = current->size;
			biggestFreeSizeAddr = current->lowAddress;
		}

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

	if ((NULL != current) && (current->size > biggestFreeSize)) {
		biggestFreeSize = current->size;
		biggestFreeSizeAddr = current->lowAddress;
	}
	
	/* We must insert node right at the end of the list */
	if (!placed) {
		FreeList *node = createNewNode(startAddress, size);
		previous->next = node;
	}

	totalFreeSpace += size;
	return true;
}

bool OffHeapList::isEmpty() {
	return NULL == head || NULL == head->lowAddress;
}

void OffHeapList::printFreeListStatus() {
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

void OffHeapList::printNode(FreeList *node) {
	printf("Low address: %p\n", node->lowAddress);
	printf("High address: %p\n", node->highAddress);
	printf("Free size: %zu\n", node->size);
}

OffHeapList::FreeList *OffHeapList::createNewNode(void *startAddress, uintptr_t size) {
	FreeList *node = new FreeList;
	node->lowAddress = startAddress;
	node->highAddress = (void*)((uintptr_t)startAddress + size);
	node->size = size;
	node->next = NULL;
	nodeCount++;
	return node;
}

void OffHeapList::freeAllList() {
	FreeList *current = head;
	if (!isEmpty()) {
		 while (NULL != current) {
			FreeList *temp = current;
			current = current->next;
			delete temp;
		 }
	}
}

void testOffHeapList() {

	OffHeapList offHeapList((void *)0x01000, 1024*16);
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
	addr0 = offHeapList.findAvailableAddress(5808);
	addr1 = offHeapList.findAvailableAddress(2000);
	offHeapList.printFreeListStatus();
	ret4 = offHeapList.addEntryToFreeList(addr1, 2000);
	offHeapList.printFreeListStatus();
}

int main(int argc, char** argv) {

	//testOffHeapList();
	//return 1;
	
	if (argc != 3) {
		std::cout<<"USAGE: " << argv[0] << " seed# iterations#" << std::endl;
		std::cout << "Example: " << argv[0] << " 6363 50000" << std::endl;
		return 1;
	}
	PaddedRandom rnd;
	int seed = atoi(argv[1]);
	int iterations = atoi(argv[2]);
	rnd.setSeed(seed); // rnd.nextNatural() % FOUR_GB

	uintptr_t iterArrayletSize[iterations];
	uintptr_t biggestFreeSize[iterations];
	void *biggestFreeSizeAddrs[iterations];

	size_t pagesize = getpagesize(); // 4096 bytes
	std::cout << "System page size: " << pagesize << " bytes.\n";
	size_t arrayletSize = getArrayletSize(pagesize) * 4;
	std::cout << "Arraylet size: " << arrayletSize << " bytes" << std::endl;
	uintptr_t regionCount = 1024;
	//uintptr_t offHeapRegionSize = FOUR_GB;
	uintptr_t inHeapSize = FOUR_GB + TWO_GB;
	uintptr_t offHeapSize = inHeapSize * 3;
	uintptr_t inHeapRegionSize = inHeapSize / regionCount;
	uintptr_t offHeapRegionSize = inHeapRegionSize;
	ElapsedTimer timer;
	timer.startTimer();

	int mmapProt = 0;
	int mmapFlags = 0;

	mmapProt = PROT_NONE; //PROT_READ | PROT_WRITE;
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

	if (inHeapMmap == MAP_FAILED) {
		std::cerr << "Failed to mmap in-heap " << strerror(errno) << "\n";
		return 1;
	}

	if (offHeapMmap == MAP_FAILED) {
		std::cerr << "Failed to mmap off-heap " << strerror(errno) << "\n";
		return 1;
	} else {
		std::cout << "Successfully mmaped off-heap at address: " << (void *)offHeapMmap << "\n";
	}
	printf("In-heap address: %p, Off-heap address: %p\n", inHeapMmap, offHeapMmap);

	/*
	mprotect(inHeapMmap, inHeapSize, PROT_READ | PROT_WRITE);
	memset(inHeapMmap, 'a', inHeapSize);
	munmap(inHeapMmap, inHeapSize);
	return 1;
	*/

	OffHeapList offHeapList(offHeapMmap, offHeapSize);
	OffHeapObjectList objList;

	mmapProt = PROT_READ | PROT_WRITE;

	/* Setup in-heap with commited regions: */
	uintptr_t numOfComitRegions = 32;
	char vals[SIXTEEN] = {'3', '5', '6', '8', '9', '0', '1', '2', '3', '7', 'A', 'E', 'C', 'B', 'D', 'F'};
	uintptr_t regionIndexes[numOfComitRegions] = {743, 34, 511, 2, 970, 888, 32, 100, 0, 123, 444, 3, 19, 721, 344, 471, 74, 234, 11, 20, 70, 188, 320, 150, 105, 203, 399, 32, 51, 727, 841, 47};
	void *leafAddresses[regionCount];
	bool inOffHeapUsed[regionCount];
	uintptr_t totalCalculatedComitedMem = 0;
	/* Populate in-heap regions */
	for(int i = 0; i < regionCount; i++) {
		void *chosenAddress = (void*)((uintptr_t)inHeapMmap + (i * inHeapRegionSize));
		leafAddresses[i] = chosenAddress;
		inOffHeapUsed[i] = false;
		//printf("\tPopulating address: %p with A's\n", chosenAddress);
		mprotect(chosenAddress, inHeapRegionSize, mmapProt);
		memset(chosenAddress, vals[i%SIXTEEN], inHeapRegionSize);
		totalCalculatedComitedMem += inHeapRegionSize;
	}

	std::cout << "************************************************\n";
        char *someString = (char*)inHeapMmap;
        printf("Chars at: 0: %c, 100: %c, 500: %c, 1024: %c, 1048576: %c\n", *(someString + regionIndexes[0]*inHeapRegionSize), *(someString + regionIndexes[1]*inHeapRegionSize + 1024), *(someString + regionIndexes[2]*inHeapRegionSize + 10000), *(someString + regionIndexes[3]*inHeapRegionSize + 20000), *(someString + regionIndexes[6]*inHeapRegionSize + 50000));
	printf("######## Calculated commited memory: %zu bytes ##########\n", totalCalculatedComitedMem);

	printf("Sleeping for 15 seconds before we start doing anything. off-heap created successfully. Fetch RSS\n");
	//sleep(15);

	uintptr_t offsets[SIXTEEN] = {0, 12, 32, 45, 100, 103, 157, 198, 234, 281, 309, 375, 416, 671, 685, 949};
	size_t totalArraySize = 0;

	/* In-heap is fully committed by now. */
	std::cout << "************************************************\n";

	/* Make sure we can read and write from this memory that we'll touch */
	int64_t elapsedTime3, elapsedTime4, elapsedTime5, elapsedTime6, elapsedTime7, elapsedTime8;
	uintptr_t totalOffHeapCommited = 0;
	void *usedOffHeapAddr[regionCount/2];
	bool sizeSwitch = false;

	// Insert big loop here to simulate decommiting and commiting of in-heap, off-heap memory respectively. Make it random?
	for(int i = 0; i < iterations; i++) {
		if(i % 7 == 0) {
			sizeSwitch = !sizeSwitch;
		}
		uintptr_t numOfRegionsAlloc = sizeSwitch ? ((rnd.nextNatural() % 8) + 2) : ((rnd.nextNatural() % 23) + 10); // Numbers between 2 and 32 included
		uintptr_t commitSize = numOfRegionsAlloc * inHeapRegionSize;
		printf("Chosen region count: %zu, commitSize: %zu, totalOffHeapCommited: %zu\n", numOfRegionsAlloc, commitSize, totalOffHeapCommited);
		/* Decommit in-heap & commit off-heap regions until we deplit in-heap memory size */
		/* E.g. If inHeapSize = 1024MB, totalOffHeapCommited = 968MB, commitSize = 82MB it will surpass maximum allowed */
		while (totalOffHeapCommited + commitSize < inHeapSize) {
			uintptr_t remmainingByes = commitSize;
			int j = 0;
			/* Decommit in-heap regions */
			while (remmainingByes > 0) {
				void *chosenAddress = NULL;
				if(!inOffHeapUsed[j]) {
					chosenAddress = leafAddresses[j];
					remmainingByes -= inHeapRegionSize;
					intptr_t ret = (intptr_t)madvise(chosenAddress, inHeapRegionSize, MADV_DONTNEED);
					if (0 != ret) {
						printf("madvise returned -1 and errno: %d, error message: %s\n", errno, strerror(errno));
						return 1;
					}
					inOffHeapUsed[j] = true;
				}
				j++;
			}
			//offHeapList.printFreeListStatus();
			/* Record returned address to keep track of object order */
			void *addr = offHeapList.findAvailableAddress(commitSize);
			if (NULL == addr) {
				printf("Failed while finding big enough memory range at off-heap! Requested size: %zu\n", commitSize);
				offHeapList.printFreeListStatus();
				return 1;
			}
			/* Commit off-heap region */
			mprotect(addr, commitSize, mmapProt);
			memset(addr, vals[i % SIXTEEN], commitSize);
			totalOffHeapCommited += commitSize;
			objList.addObjToList(addr, commitSize);
		}

		/* Free half of off-heap. For every other address */
		objList.printOffHeapObjectStatus();
		uintptr_t offHeapBytesFreed = objList.removeHalfOfNodes(&offHeapList);
		if (UINTMAX_MAX == offHeapBytesFreed) {
			printf("Something went wrong while freeing half of off-heap objects\n");
			return 1;
		}
		uintptr_t regionsFreedCount = offHeapBytesFreed / inHeapRegionSize;
		/* Recommit in-heap memory */
		uintptr_t oind = 0;
		for (int j = 0; j < regionsFreedCount; j++) {
			while(!inOffHeapUsed[oind]) oind++;
			inOffHeapUsed[oind] = false;
			void *chosenAddress = leafAddresses[oind];
			intptr_t ret = (intptr_t)madvise(chosenAddress, inHeapRegionSize, MADV_WILLNEED);
			if (0 != ret) {
				printf("madvise returned -1 trying to recommit memory and errno: %d, error message: %s\n", errno, strerror(errno));
				return 1;
			}
			memset(chosenAddress, vals[oind%SIXTEEN], inHeapRegionSize);
		}
		totalOffHeapCommited -= offHeapBytesFreed;
		objList.printOffHeapObjectStatus();
		offHeapList.printFreeListStatus();

		printf("Sleeping for 2 seconds after iter: %d\n", i);
		biggestFreeSize[i] = offHeapList.getApproximateBiggestFreeSize();
		iterArrayletSize[i] = numOfRegionsAlloc;
		biggestFreeSizeAddrs[i] = offHeapList.getBiggestFreeSizeAddr();
		sleep(2);
	}

	// ###############################################

	//printf("Time taken to mprotect off_heap region %zu: %.2f us, in seconds: %.2f\n", totalCalculatedComitedMem, mprotectTime2, (mprotectTime2/1000000));
	//printf("Time taken to memset %zu at off-heap with 2's: %.2f us, in seconds: %.2f\n", totalCalculatedComitedMem, memsetTime2, (memsetTime2/1000000));

	printf("########################################### SUMMARY #####################################################\n");
	printf("Total iterations: %d\n", iterations);
	uintptr_t smallestBiggestSize = UINTMAX_MAX;
	int chosenIter = 0;
	for (int i = 0; i < iterations; i++) {
		printf("Iteration: %d, biggest free size addr: %p, biggest free size: %zu, region size: %zu\n", i, biggestFreeSizeAddrs[i], biggestFreeSize[i], iterArrayletSize[i]);
		if (biggestFreeSize[i] < smallestBiggestSize) {
			smallestBiggestSize = biggestFreeSize[i];
			chosenIter = i;
		}
	}
	printf("The smallest biggest free region found was at iteration: %d with size: %zu bytes\n", chosenIter, smallestBiggestSize);
	printf("##########################################################################################################\n");

	munmap(inHeapMmap, inHeapSize);
	munmap(offHeapMmap, offHeapSize);
	printf("Sleeping for 5 seconds unmapping off-heap. Fetch RSS\n");
	//sleep(5);

    return 0;
}

#endif /* OFF_HEAP_SIMULATION */

