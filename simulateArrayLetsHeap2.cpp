#ifndef SIMULATE_ARRAYLETS_HEAP2
#define SIMULATE_ARRAYLETS_HEAP2

#include <iostream>
#include <fstream>
#include <string>

#include "util.hpp"

// To run:
// g++ -g3 -Wno-write-strings -std=c++11 simulateArrayLetsHeap2.cpp -o simulateArrayLetsHeap2
// Note: Insert -lrt flag for linux systems
// ./simulateArrayLetsHeap2 12 1000

char * mmapContiguous(size_t totalArraySize, size_t arrayletSize, int fhs[], char * addresses[])
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
    
    addresses[ARRAYLET_COUNT] = contiguousMap;

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

char * copyModifyManualHeap(size_t pagesize, size_t arrayletSize, size_t totalArraySize, long arrayLetOffsets[], char * heapMmap) 
   {
    char * tempArray = (char *)malloc(totalArraySize);
    
    // for(size_t i = 0; i < ARRAYLET_COUNT; i++) {
    //     for(size_t j = 0; j < arrayletSize; j++)
    //     {
    //         tempArray[j+(i*arrayletSize)] = (heapMmap+arrayLetOffsets[i])[j];
    //     }
    // }

    for(size_t i = 0; i < ARRAYLET_COUNT; i++) {
        std::memcpy(tempArray+(i*arrayletSize), heapMmap+arrayLetOffsets[i],arrayletSize);
    }

    // Modify temporary array with asterisks  
   for(size_t i = 0; i < 256; i++) {
       for(size_t j = 0; j < ARRAYLET_COUNT; j++)
        {
        tempArray[i+j*arrayletSize] = '*';
        tempArray[i+j*arrayletSize+(arrayletSize/4)] = '*';
        tempArray[i+j*arrayletSize+(arrayletSize/4)] = '*';
        }
   }

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

   for(size_t i = 0; i < ARRAYLET_COUNT; i++) {
       std::memcpy(heapMmap+arrayLetOffsets[i], tempArray+(i*arrayletSize), arrayletSize);
       (heapMmap+arrayLetOffsets[i])[arrayletSize-2] = '\0';
   }

   return tempArray;

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
    size_t arrayletSize = getArrayletSize(pagesize);
    std::cout << "arrayletSize size: " << arrayletSize << " bytes.\n";

    // 1. Simulate heap by allocating 256MB of memory
    //    No read, write or exec priviledges given 

    char * heapMmap = (char *)mmap(
                NULL,
                FOUR_GB, // File size
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


    char * tempNums[SIXTEEN] = {"33", "55", "66", "88", "99", "00", "11", "22", "33", "77", "AA", "EE", "CC", "BB", "DD", "FF"};

    char * arrayletNames[ARRAYLET_COUNT];
    char * nums[ARRAYLET_COUNT];
    long arrayLetOffsets[ARRAYLET_COUNT];

    for(size_t i = 0; i < ARRAYLET_COUNT; i++) {
        std::string numStr = std::to_string(i);
        numStr = "t" + numStr;
        arrayletNames[i] = strdup(numStr.c_str());
        nums[i] = tempNums[i%SIXTEEN];
        arrayLetOffsets[i] = getPageAlignedOffset(pagesize, rnd.nextNatural() % FOUR_GB);
    }

    
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
    char * addresses[ARRAYLET_COUNT + 1];
    double perIter[iterations], ignoreTimes[iterations];

    // char * contiguousMap = mmapContiguous(totalArraySize, arrayletSize, fhs, addresses);
    // modifyContiguousMem(pagesize, arrayletSize, contiguousMap);

    // copyModifyManualHeap(pagesize, totalArraySize, arrayLetOffsets, heapMmap);

    timer.startTimer();
    double lastTime = timer.getElapsedMillis();
    double middleTime = lastTime;

    for(size_t i = 0; i < iterations; i++) {
        // 3. Make Arraylets look contiguous with mmap
        char * contiguousMap = mmapContiguous(totalArraySize, arrayletSize, fhs, addresses);

        // ************************************************************************************************
        // 4. Modify contiguous memory view and observe change in the heap
        modifyContiguousMem(pagesize, arrayletSize, contiguousMap);

        // Free addresses
        middleTime = timer.getElapsedMillis();
        freeAddresses(addresses, arrayletSize);

        // // 3. 4. Both copy arraylets into a separate array, modify this array to then copy it back to the heap
        // char * allocArray = copyModifyManualHeap(pagesize, arrayletSize, totalArraySize, arrayLetOffsets, heapMmap);
        // middleTime = timer.getElapsedMillis();
        // freeAllocArray((void *)allocArray);

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
    
    // Prints arraylets from heap location
    // for(size_t i = 0; i < ARRAYLET_COUNT; i++) {   
    //     std::cout << "heapMmap+arrayLetOffsets[" << i << "]: ";
    //     for(size_t j = 0; j < arrayletSize; j++) {  
    //         std::cout << (heapMmap+arrayLetOffsets[i])[j];
    //     }
    //     std::cout << std::endl;
    // }

    munmap(heapMmap, FOUR_GB);

    return 0;
}

#endif /* SIMULATE_ARRAYLETS_HEAP2 */