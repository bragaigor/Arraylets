#ifndef SIMULATE_ARRAYLETS_HEAP
#define SIMULATE_ARRAYLETS_HEAP

#include <iostream>
#include <fstream>
#include <string>

#include "util.hpp"

// TODO try HUGE_TLB passed to mmap - MACOSX does not suport MAP_HUGETLB
// TODO Code cleanup
// TODO run approach 1 on linux. Use Ubuntu 16.04 on docker

// To run:
// g++ -g3 -Wno-write-strings -std=c++11 simulateArrayLetsHeap.cpp -o simulateArrayLetsHeap
// Note: Insert -lrt flag for linux systems
// ./simulateArrayLetsHeap 12 1000

char * mmapContiguous(size_t totalArraySize, size_t arrayletSize, long arrayLetOffsets[], int fh, char * addresses[])
   {
    char * contiguousMap = (char *)mmap(
                   NULL,
                   totalArraySize, // File size
                   PROT_READ|PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON, // Must be shared
                   -1,
                   0);

    if (contiguousMap == MAP_FAILED) {
      std::cerr << "Failed to mmap contiguousMap\n";
      return NULL;
    }

    addresses[ARRAYLET_COUNT] = contiguousMap;

    for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
       void *nextAddress = (void *)(contiguousMap+i*arrayletSize);
       addresses[i] = (char *)mmap(
                   nextAddress,
                   arrayletSize, // File size
                   PROT_READ|PROT_WRITE,
                   MAP_SHARED | MAP_FIXED,
                   fh,
                   arrayLetOffsets[i]);

        if (addresses[i] == MAP_FAILED) {
            std::cerr << "Failed to mmap address[" << i << "] at mmapContiguous()\n";
            munmap(contiguousMap, totalArraySize);
            contiguousMap = NULL;
        } else if (nextAddress != addresses[i]) {
            std::cerr << "Map failed to provide the correct address. nextAddress " << nextAddress << " != " << addresses[i] << std::endl;
            munmap(contiguousMap, totalArraySize);
            contiguousMap = NULL;
        }
    }

     return contiguousMap;
   }

/**
 * Uses shm_open to create dummy empty file and
 * ftruncate to allocate desired memory size.
 */

int main(int argc, char** argv) {

    if (argc != 3) {
        std::cout<<"USAGE: " << argv[0] << " seed# iterations#" << std::endl;
        std::cout << "Example: " << argv[0] << " 6363 50000" << std::endl;
        return 1;
    }
    
    PaddedRandom rnd;
    int seed = atoi(argv[1]);
    int iterations = atoi(argv[2]);
    rnd.setSeed(seed);

    size_t pagesize = getpagesize(); // 4096 bytes
    std::cout << "System page size: " << pagesize << " bytes.\n";
    size_t arrayletSize = getArrayletSize(pagesize);
    std::cout << "Arraylet size: " << arrayletSize << " bytes" << std::endl;

    char * filename = "temp.txt";
    int fh = shm_open(filename, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fh == -1) {
      std::cerr << "Error while reading file" << filename << "\n";
      return 1;
    }
    shm_unlink(filename);

    // Sets the desired size to be allocated
    // Failing to allocate memory will result in a bus error on access.
    ftruncate(fh, FOUR_GB);

    char * heapMmap = (char *)mmap(
                NULL,
                FOUR_GB, // File size
                PROT_READ|PROT_WRITE,
                MAP_SHARED, // Must be shared
                fh, // File handle
                0);

    if (heapMmap == MAP_FAILED) {
       std::cerr << "Failed to mmap\n";
       return 1;
    } else {
       std::cout << "Successfully mmaped heap at address: " << (void *)heapMmap << "\n";
    }

    long * heapAddr = (long *)heapMmap;
    std::cout << "Heap address converted to char* is: " << heapAddr+128 << std::endl;

    // Get page alligned offsets
    long arrayLetOffsets[ARRAYLET_COUNT];
    for(size_t i = 0; i < ARRAYLET_COUNT; i++) {
        arrayLetOffsets[i] = getPageAlignedOffset(arrayletSize, rnd.nextNatural() % FOUR_GB);
    }

    for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
       std::cout << "Arralylet at " << i << " has offset: " << arrayLetOffsets[i] << '\n';
    }

    char * tempNums[SIXTEEN] = {"33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF"};

    char * nums[ARRAYLET_COUNT];
    
    size_t totalArraySize = 0;

    for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
       nums[i] = tempNums[i%SIXTEEN];
       for (size_t j = 0; j < arrayletSize; j++) {
          strncpy(heapMmap+arrayLetOffsets[i]+j, nums[i], 1);
          totalArraySize++;
       }
    }
    std::cout << "Arraylets created successfully.\n";
    std::cout << "ArrayLets combined have size: " << totalArraySize << " bytes." << '\n';

    char * addresses[ARRAYLET_COUNT + 1];
    double totalMapTime = 0;
    double totalModifyTime = 0;
    double totalFreeTime = 0;

    ElapsedTimer timer;
    timer.startTimer();

    for(size_t i = 0; i < iterations; i++) {
        double start = timer.getElapsedMicros();
        // 3. Make Arraylets look contiguous with mmap
        char * contiguousMap = mmapContiguous(totalArraySize, arrayletSize, arrayLetOffsets, fh, addresses);

        double mapEnd = timer.getElapsedMicros();

        // 4. Modify contiguous memory view and observe change in the heap
        modifyContiguousMem(pagesize, arrayletSize, contiguousMap);

        double modifyEnd = timer.getElapsedMicros();

        // Free addresses
        munmap(contiguousMap, totalArraySize);

        double freeEnd = timer.getElapsedMicros();

        totalMapTime += (mapEnd - start);
        totalModifyTime += (modifyEnd - mapEnd);
        totalFreeTime += (freeEnd - modifyEnd);
    }

    int64_t elapsedTime = timer.getElapsedMicros();
    
    std::cout << "Test completed " << iterations << " iterations" << std::endl;
    std::cout << "Total elapsed time " << elapsedTime << "us" << std::endl;
    std::cout << "Total map time " << totalMapTime << "us AVG map time " << (totalMapTime / iterations) << "us" << std::endl;
    std::cout << "Total modify time " << totalModifyTime << "us AVG modify time " << (totalModifyTime / iterations) << "us" << std::endl;
    std::cout << "Total free time " << totalFreeTime << "us AVG free time " << (totalFreeTime / iterations) << "us" << std::endl;

    munmap(heapMmap, FOUR_GB);

    return 0;
}

#endif /* SIMULATE_ARRAYLETS_HEAP */
