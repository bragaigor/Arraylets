#ifndef SIMULATE_ARRAYLETS_HEAP
#define SIMULATE_ARRAYLETS_HEAP

#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <fstream>
#include <string>
#include <cstring>

#include "util.hpp"

// To run:
// g++ -g3 -Wno-write-strings -std=c++11 simulateArrayLetsHeap.cpp -o simulateArrayLetsHeap
// ./simulateArrayLetsHeap 12 1000

#define ARRAYLET_COUNT 64
#define TWO_HUNDRED_56_MB 256000000
#define ONE_GB 1000000000 // 1GB

char ** getArrayLets(size_t pagesize)
   {
   char** arrayLets = new char*[ARRAYLET_COUNT];

   char * array1 = new char[pagesize * 4];
   char * padding0 = new char[pagesize * 16];
   char * array4 = new char[pagesize * 4];
   char * padding1 = new char[pagesize * 16];
   char * array5 = new char[pagesize * 4];
   char * padding2 = new char[pagesize * 16];
   char * array3 = new char[pagesize * 4];
   char * padding3 = new char[pagesize * 16];
   char * array2 = new char[pagesize * 4];

   for (size_t i = 0; i < pagesize*4; i++) {
      array1[i] = '1';
      array4[i] = '4';
      array3[i] = '3';
      array2[i] = '2';
      array5[i] = '5';
   }

   arrayLets[0] = array4;
   arrayLets[1] = array5;
   arrayLets[2] = array3;
   arrayLets[3] = array1;
   arrayLets[4] = array2;

   delete [] padding0;
   delete [] padding1;
   delete [] padding2;
   delete [] padding3;

   return arrayLets;
   }

long getPageAlignedOffset(size_t pagesize, long num)
   {
   int remain = num % pagesize;
   if(remain < pagesize / 2)
      return num - remain;
   else
      return num + (pagesize - remain);
   }

void dealocateArrayLets(char** arrayLets)
   {
   delete [] arrayLets;
   }

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

    // char * addresses[ARRAYLET_COUNT];

    for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
        
       addresses[i] = (char *)mmap(
                   (void *)(contiguousMap+i*arrayletSize),
                   arrayletSize, // File size
                   PROT_READ|PROT_WRITE,
                   MAP_SHARED | MAP_FIXED,
                   fh,
                   arrayLetOffsets[i]);

        if (addresses[i] == MAP_FAILED) {
            std::cerr << "Failed to mmap leaf\n";
            return NULL;
        } 
        // else {
        //     std::cout << "Successfully mmaped leaf at address: " << (void *)addresses[i] << " :" << *addresses[i] << "\n";
        // }
    }

     return contiguousMap;
   }

void modifyContiguousMem(size_t pagesize, size_t arrayletSize, char * contiguousMap) 
   {
    for(size_t i = 0; i < 256; i++) {
        
        for(size_t j = 0; j < ARRAYLET_COUNT; j++)
            {
            contiguousMap[i+j*arrayletSize+pagesize] = '*';
            }
    
    }
   }

void freeAddresses(char * addresses[], size_t arrayletSize) {
    for (size_t i = 0; i < ARRAYLET_COUNT+1; i++) {
        // std::cout << "Address[" << i << "]: " << (void *)addresses[i] << "\n";
        munmap(addresses[i], arrayletSize);
    }
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
    size_t arrayletSize = pagesize*16; // 4096 * 16 * 16 = 1MB

    char * filename = "temp.txt";
    int fh = shm_open(filename, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fh == -1) {
      std::cerr << "Error while reading file" << filename << "\n";
      return 1;
    }
    shm_unlink(filename);

    // Sets the desired size to be allocated
    // Failing to allocate memory will result in a bus error on access.
    ftruncate(fh, ONE_GB);

    char * heapMmap = (char *)mmap(
                NULL,
                ONE_GB, // File size
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
        arrayLetOffsets[i] = getPageAlignedOffset(pagesize, rnd.nextNatural() % ONE_GB);
    }

    for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
       std::cout << "Arralylet at " << i << " has offset: " << arrayLetOffsets[i] << '\n';
    }

    // strncpy(heapMmap+arrayLetOffsets[0], "****************************************", 16);
    // std::cout << "heapMmap+arrayLetOffsets[0]: " << heapMmap+arrayLetOffsets[0] << '\n';

    char * nums[ARRAYLET_COUNT] = {"33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF",
                                   "33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF",
                                   "33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF",
                                   "33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF"};
                                //    "33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF",
                                //    "33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF",
                                //    "33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF",
                                //    "33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF"};
    size_t totalArraySize = 0;

    for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
       for (size_t j = 0; j < arrayletSize; j++) {
          strncpy(heapMmap+arrayLetOffsets[i]+j, nums[i], 1);
          totalArraySize++;
       }
    }
    // std::cout << "Arraylet 3: " << heapMmap+arrayLetOffsets[3] << "\n";
    std::cout << "Arraylets created successfully.\n";
    std::cout << "ArrayLets combined have size: " << totalArraySize << " bytes." << '\n';

    ElapsedTimer timer;
    timer.startTimer();
    double perIter[iterations];
    double lastTime = timer.getElapsedMillis();

    char * addresses[ARRAYLET_COUNT + 1];

    // char * contiguousMap = mmapContiguous(totalArraySize, arrayletSize, arrayLetOffsets, fh, addresses);
    // modifyContiguousMem(pagesize, arrayletSize, contiguousMap);
    // freeAddresses(addresses, arrayletSize);

    for(size_t i = 0; i < iterations; i++) {
        // 3. Make Arraylets look contiguous with mmap
        char * contiguousMap = mmapContiguous(totalArraySize, arrayletSize, arrayLetOffsets, fh, addresses);

        // 4. Modify contiguous memory view and observe change in the heap
        modifyContiguousMem(pagesize, arrayletSize, contiguousMap);
        // Free addresses
        freeAddresses(addresses, arrayletSize);

        perIter[i] = timer.getElapsedMillis() - lastTime;
        lastTime = timer.getElapsedMillis();
    }

    int64_t elapsedTime = timer.getElapsedMillis();
    std::cout << "Total time spent to create and modify both contiguous and heap locations: " 
              << elapsedTime << " microseconds (" << elapsedTime/1000000.0 << " seconds)" << "\n"; 

    size_t perIterationSum = 0;
    for(size_t i = 0; i < iterations; i++) {
        perIterationSum += perIter[i];
    }
    size_t avgPerIter = perIterationSum / iterations;
    std::cout << "Average time per iteration is: " << avgPerIter << " microseconds.\n";

    std::cout << "NOTE: 1 second = 10^6 microseconds.\n";
    
    // Print arraylets from heap
    // for(size_t i = 0; i < ARRAYLET_COUNT; i++)
    // {
    //     std::cout << "heapMmap+arrayLetOffsets[" << i << "]: " << heapMmap+arrayLetOffsets[i] << '\n';
    // }

    munmap(heapMmap, ONE_GB);

    return 0;
}

#endif /* SIMULATE_ARRAYLETS_HEAP */