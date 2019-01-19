#ifndef SIMULATE_ARRAYLETS_HEAP
#define SIMULATE_ARRAYLETS_HEAP

#include <iostream>
#include <fstream>
#include <string>

#include "util.hpp"

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
    // else {
    //   std::cout << "Successfully mmaped contiguousMap at address: " << (void *)contiguousMap << "\n";
    // }

    addresses[ARRAYLET_COUNT] = contiguousMap;

    for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
        
       addresses[i] = (char *)mmap(
                   (void *)(contiguousMap+i*arrayletSize),
                   arrayletSize, // File size
                   PROT_READ|PROT_WRITE,
                   MAP_SHARED | MAP_FIXED,
                   fh,
                   arrayLetOffsets[i]);

        if (addresses[i] == MAP_FAILED) {
            std::cerr << "Failed to mmap address[" << i << "] at mmapContiguous()\n";
            return NULL;
        } 
        // else {
        //     std::cout << "Successfully mmaped leaf at address: " << (void *)addresses[i] << " :" << *addresses[i] << "\n";
        // }
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

    char * filename = "temp.txt";
    int fh = shm_open(filename, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fh == -1) {
      std::cerr << "Error while reading file" << filename << "\n";
      return 1;
    }
    shm_unlink(filename);

    // Sets the desired size to be allocated
    // Failing to allocate memory will result in a bus error on access.
    ftruncate(fh, TWO_HUNDRED_56_MB);

    char * heapMmap = (char *)mmap(
                NULL,
                TWO_HUNDRED_56_MB, // File size
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
        arrayLetOffsets[i] = getPageAlignedOffset(pagesize, rnd.nextNatural() % TWO_HUNDRED_56_MB);
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

    ElapsedTimer timer;
    timer.startTimer();
    double perIter[iterations], ignoreTimes[iterations];
    double lastTime = timer.getElapsedMillis();
    double middleTime = lastTime;

    char * addresses[ARRAYLET_COUNT + 1];

    // char * contiguousMap = mmapContiguous(totalArraySize, arrayletSize, arrayLetOffsets, fh, addresses);
    // modifyContiguousMem(pagesize, arrayletSize, contiguousMap);

    for(size_t i = 0; i < iterations; i++) {
        // 3. Make Arraylets look contiguous with mmap
        char * contiguousMap = mmapContiguous(totalArraySize, arrayletSize, arrayLetOffsets, fh, addresses);

        // 4. Modify contiguous memory view and observe change in the heap
        modifyContiguousMem(pagesize, arrayletSize, contiguousMap);
        // Free addresses
        middleTime = timer.getElapsedMillis();
        freeAddresses(addresses, arrayletSize);

        perIter[i] = timer.getElapsedMillis() - lastTime;
        lastTime = timer.getElapsedMillis();
        ignoreTimes[i] = lastTime - middleTime;
    }

    int64_t elapsedTime = timer.getElapsedMillis();

    size_t perIterationSum = 0;
    size_t ignoreTotal = 0;
    for(size_t i = 0; i < iterations; i++) {
        perIterationSum += perIter[i];
        ignoreTotal += ignoreTimes[i];
    }
    size_t avgPerIter = perIterationSum / iterations;
    size_t avgIgnore = ignoreTotal / iterations;
    
    printResults(elapsedTime, ignoreTotal, avgPerIter, avgIgnore);
    
    // Print arraylets from heap
    // for(size_t i = 0; i < ARRAYLET_COUNT; i++)
    // {
    //     std::cout << "heapMmap+arrayLetOffsets[" << i << "]: " << heapMmap+arrayLetOffsets[i] << '\n';
    // }

    munmap(heapMmap, TWO_HUNDRED_56_MB);

    return 0;
}

#endif /* SIMULATE_ARRAYLETS_HEAP */