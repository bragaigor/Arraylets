
#include <iostream>
#include <fstream>

class OffHeapList;

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
		head = NULL;
	}

	~OffHeapObjectList() {
		freeAllList();
	}

	bool isEmpty();
	void printOffHeapObjectStatus();
	uintptr_t removeHalfOfNodes(OffHeapList *offHeapList);
	bool addObjToList(void *address, uintptr_t size);

private:

	ObjectAddrNode *createNewNode(void *address, uintptr_t size);
	void printfObject(ObjectAddrNode *node);
	void freeAllList();

	uintptr_t nodeCount;
	uintptr_t objTotalSize;
	ObjectAddrNode *head;
};

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
		totalFreeSpace(size),
		biggestFreeSize(0),
		biggestFreeSizeAddr(NULL)
	{
		initFreeList(startAddress, size);
	}

	/* Destructor */
	~OffHeapList() {
		printf("Destructor called. Freeing all nodes in FreeList\n");
		freeAllList();
	}

	void initFreeList(void *startAddress, uintptr_t size);
	void *findAvailableAddress(uintptr_t size);
	bool addEntryToFreeList(void *startAddress, uintptr_t size);
	bool isEmpty();
	void printFreeListStatus();

	uintptr_t getApproximateBiggestFreeSize() {
		return biggestFreeSize;
	}

	void *getBiggestFreeSizeAddr() {
		return biggestFreeSizeAddr;
	}

private:
	void printNode(FreeList *node);
	FreeList *createNewNode(void *startAddress, uintptr_t size);
	void freeAllList();

	FreeList *head;
	uintptr_t nodeCount;
	uintptr_t totalFreeSpace;
	uintptr_t biggestFreeSize;
	void *biggestFreeSizeAddr;
};
