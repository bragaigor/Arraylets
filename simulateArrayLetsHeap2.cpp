#ifndef SIMULATE_ARRAYLETS_HEAP2
#define SIMULATE_ARRAYLETS_HEAP2

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

#include "util.hpp"

// To run:
// g++ -g3 -Wno-write-strings -std=c++11 simulateArrayLetsHeap2.cpp -o simulateArrayLetsHeap2
// ./simulateArrayLetsHeap2 12 1000

#define ARRAYLET_COUNT 32
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

char * mmapContiguous(size_t totalArraySize, size_t arrayletSize, int fhs[])
   {
    char * contiguousMap = (char *)mmap(
                   NULL,
                   totalArraySize, // File size
                   PROT_READ|PROT_WRITE,
                   MAP_SHARED | MAP_ANON, // Must be shared
                   -1,
                   0);

    if (contiguousMap == MAP_FAILED) {
       std::cerr << "Failed to mmap contiguousMap\n";
       return NULL;
    } 
    // else {
    //    std::cout << "Successfully mmaped contiguousMap at address: " << (void *)contiguousMap << "\n";
    // }
    
    char * addresses[ARRAYLET_COUNT];

    for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
        
       addresses[i] = (char *)mmap(
                   (void *)(contiguousMap+i*arrayletSize),
                   arrayletSize, // File size
                   PROT_READ|PROT_WRITE,
                   MAP_SHARED | MAP_FIXED,
                   fhs[i],
                   0);

        if (addresses[i] == MAP_FAILED) {
            std::cout << "Failed to mmap addresses[" << i << "]\n";
            return NULL;
        } 
        // else {
        //     std::cout << "Successfully mmaped leaf at address: " << i << ": " << (void *)addresses[i] << " :" << *addresses[i] << "\n";
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

void copyModifyManualHeap(size_t pagesize, size_t arrayletSize, size_t totalArraySize, long arrayLetOffsets[], char * heapMmap) 
   {
   char tempArray[totalArraySize];

   for(size_t i = 0; i < ARRAYLET_COUNT; i++) {
       
    //    for(size_t j = 0; j < arrayletSize; j++)
    //    {
    //        tempArray[j+(i*arrayletSize)] = (heapMmap+arrayLetOffsets[i])[j];
    //    }
       std::memcpy(tempArray+(i*arrayletSize), heapMmap+arrayLetOffsets[i],arrayletSize);
   }

    // Modify temporary array with asterisks  
   for(size_t i = 0; i < 256; i++) {
       for(size_t j = 0; j < ARRAYLET_COUNT; j++)
        {
        tempArray[i+j*arrayletSize+pagesize] = '*';
        }
   }

//    std::cout << "tempArray: " << tempArray << std::endl;
   // Copy tempArray back into the heap 
   int offsetIdx = 0;
   
//    for(size_t i = 0, j = 0; i < totalArraySize-ARRAYLET_COUNT; i++) {
//        (heapMmap+arrayLetOffsets[offsetIdx])[j++] = tempArray[i];

//        if(i != 0 && (i % (arrayletSize-1) == 0)) {
//            (heapMmap+arrayLetOffsets[offsetIdx])[--j] = '\0';
//            offsetIdx++;
//            j = 0;
//        }
//    }

   for(size_t i = 0; i < ARRAYLET_COUNT; i++)
   {
       std::memcpy(heapMmap+arrayLetOffsets[i], tempArray+(i*arrayletSize), arrayletSize);
       (heapMmap+arrayLetOffsets[i])[arrayletSize-2] = '\0';
   }
   

   }

/**
 * Uses shm_open and ftruncate to simulate heap as well as 
 * arraylets. Populates heap in random locations with numbers
 * to then alocate another contiguous block of memory to make
 * the arraylets look contiguous. 
 */

int main(int argc, char** argv) {

    if (argc != 3) {
        std::cout<<"USAGE: " << argv[0] << " seed# iterations#" << std::endl;
        std::cout << "Example: " << argv[0] << " 6363 50000" << std::endl;
        return 1;
    }
    
    ElapsedTimer timer;

    PaddedRandom rnd;
    int seed = atoi(argv[1]);
    int iterations = atoi(argv[2]);
    rnd.setSeed(seed);

    size_t pagesize = getpagesize(); // 4096 bytes
    std::cout << "System page size: " << pagesize << " bytes.\n";
    size_t arrayletSize = pagesize*16*16; // 4096 * 16 * 4 * 4 = 1MB

    // 1. Simulate heap by allocating 256MB of memory
    //    No read, write or exec priviledges given 

    char * heapMmap = (char *)mmap(
                NULL,
                ONE_GB, // File size
                PROT_NONE,
                MAP_SHARED | MAP_ANON, // Must be shared
                -1, // File handle
                0);

    if (heapMmap == MAP_FAILED) {
       std::cerr << "Failed to mmap\n";
       return 1;
    } else {
       std::cout << "Successfully mmaped heapMmap at address: " << (void *)heapMmap << "\n";
    }

    // 2. Populate heap in random locations with simulated arraylets

    int fhs[ARRAYLET_COUNT]; // File handles for each arraylet
    char * arrayletAddrs[ARRAYLET_COUNT];
    size_t totalArraySize = 0;

    // TODO: Change hard coded arrayletNames and nums to for loop
    char * arrayletNames[ARRAYLET_COUNT] = {"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "t9", "tA", "tB", "tC", "tD", "tE", "tF",
                                            "t00", "t10", "t20", "t30", "t40", "t50", "t60", "t70", "t80", "t90", "tA0", "tB0", "tC0", "tD0", "tE0", "tF0"};
                                            // "t01", "t11", "t21", "t31", "t41", "t51", "t61", "t71", "t81", "t91", "tA1", "tB1", "tC1", "tD1", "tE1", "tF1",
                                            // "t02", "t12", "t22", "t32", "t42", "t52", "t62", "t72", "t82", "t92", "tA2", "tB2", "tC2", "tD2", "tE2", "tF2",
                                            // "t03", "t13", "t23", "t34", "t43", "t53", "t63", "t73", "t83", "t93", "tA3", "tB3", "tC3", "tD3", "tE3", "tF3",
                                            // "t04", "t14", "t24", "t35", "t44", "t54", "t64", "t74", "t84", "t94", "tA4", "tB4", "tC4", "tD4", "tE4", "tF4",
                                            // "t05", "t15", "t25", "t36", "t45", "t55", "t65", "t75", "t85", "t95", "tA5", "tB5", "tC5", "tD5", "tE5", "tF5",
                                            // "t06", "t16", "t26", "t37", "t46", "t56", "t66", "t76", "t86", "t96", "tA6", "tB6", "tC6", "tD6", "tE6", "tF6"};
    char * nums[ARRAYLET_COUNT] = {"33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF",
                                   "33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF"};
                                //    "33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF",
                                //    "33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF",
                                //    "33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF",
                                //    "33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF",
                                //    "33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF",
                                //    "33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF"};

    // Get page alligned offsets
    long arrayLetOffsets[ARRAYLET_COUNT];

    for(size_t i = 0; i < ARRAYLET_COUNT; i++) {
        arrayLetOffsets[i] = getPageAlignedOffset(pagesize, rnd.nextNatural() % ONE_GB);
    }

    // char * arrayletNames[ARRAYLET_COUNT];
    // char * nums[ARRAYLET_COUNT];
    // long arrayLetOffsets[ARRAYLET_COUNT];

    // for(size_t i = 0; i < ARRAYLET_COUNT; i++) {
    //     std::string numStr = std::to_string(i);
    //     numStr = "t" + numStr;
    //     arrayletNames[i] = strdup(numStr.c_str());
    //     nums[i] = "11";
    //     arrayLetOffsets[i] = getPageAlignedOffset(pagesize, rnd.nextNatural() % ONE_GB);
    // }

    
    for(size_t i = 0; i < ARRAYLET_COUNT; i++)
    {
        fhs[i] = shm_open(arrayletNames[i], O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fhs[i] == -1) {
            std::cerr << "Error while reading file " << arrayletNames[i] << "\n";
            return 1;
        }
        shm_unlink(arrayletNames[i]);
        ftruncate(fhs[i], arrayletSize);

        arrayletAddrs[i] = (char *)mmap(
                (void *)(heapMmap+arrayLetOffsets[i]),
                arrayletSize, // File size
                PROT_READ|PROT_WRITE,
                MAP_SHARED | MAP_FIXED, // Must be shared
                fhs[i], // File handle
                0);

        if (arrayletAddrs[i] == MAP_FAILED) {
            std::cerr << "Failed to mmap\n";
            return 1;
        } else {
            std::cout << "Successfully mmaped arrayletAddrs[" << i << "] at address: " << (void *)arrayletAddrs[i] << "\n";
        }

        for (size_t j = 0; j < arrayletSize; j++) {
          strncpy(arrayletAddrs[i]+j, nums[i], 1);
          totalArraySize++;
        }
    }

    std::cout << "Arraylets created successfully.\n";
    std::cout << "ArrayLets combined have size: " << totalArraySize << " bytes." << '\n';
    
    // ************************************************************************************************
    // char * contiguousMap = mmapContiguous(totalArraySize, pagesize, fhs);
    // modifyContiguousMem(pagesize, arrayletSize, contiguousMap);

    // copyModifyManualHeap(pagesize, totalArraySize, arrayLetOffsets, heapMmap);

    timer.startTimer();
    double perIter[iterations];
    double lastTime = timer.getElapsedMillis();
    
    for(size_t i = 0; i < iterations; i++) {
        // // 3. Make Arraylets look contiguous with mmap
        // char * contiguousMap = mmapContiguous(totalArraySize, arrayletSize, fhs);

        // // ************************************************************************************************
        // // 4. Modify contiguous memory view and observe change in the heap
        // modifyContiguousMem(pagesize, arrayletSize, contiguousMap);

        // 3. 4. Both copy arraylets into a separate array, modify this array to then copy it back to the heap
        copyModifyManualHeap(pagesize, arrayletSize, totalArraySize, arrayLetOffsets, heapMmap);

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
    
    // Required for printing to standard output 
    // for(size_t i = 1; i < ARRAYLET_COUNT; i++)
    // {
    //     contiguousMap[i*arrayletSize-2] = '\0';
    // }
    
    // Prints arraylets from heap location
    // for(size_t i = 0; i < ARRAYLET_COUNT; i++)
    // {
    //     std::cout << "heapMmap+arrayLetOffsets[" << i << "]: " << heapMmap+arrayLetOffsets[i] << '\n';
    // }

    munmap(heapMmap, ONE_GB);

    return 0;
}

#endif /* SIMULATE_ARRAYLETS_HEAP2 */