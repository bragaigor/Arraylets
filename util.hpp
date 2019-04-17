#ifndef UTIL_H
#define UTIL_H

#ifdef LINUX_ARRAYLET

#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

#endif

#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>

#include <chrono>
#include <iostream>
#include <cstring>

#define ARRAYLET_COUNT 16
#define ARRAYLET_SIZE_CONST 1024 // (pagesize)4096(POSIX) | 65536(Windows) * ARRAYLET_SIZE_CONST: 64 KB
#define SIXTEEN 16
#define TWO_HUNDRED_56_MB 268435456
#define ONE_GB 1073741824 // 1GB
#define TWO_GB 2147483648 // 2GB
#define FOUR_GB 4294967296 // 4GB
#define EIGHT_GB 8589934592 // 8GB
#define SIXTEEN_GB 17179869184 // 16GB
#define SIXTY_FOUR_GB 68719476736 // 64GB
#define PADDING_BYTES 128

#define MMAP_FLAG_SHARED_ANON  1
#define MMAP_FLAG_PRIVATE_ANON  2
#define MMAP_FLAG_ANON_TLB  4
#define MMAP_FLAG_SHARED 8
#define MMAP_FLAG_SHARED_FIXED  16
#define MMAP_FLAG_PRIVATE_FIXED  32


class ElapsedTimer {
private:
    char padding0[PADDING_BYTES];
    bool calledStart;
    char padding1[PADDING_BYTES];
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
    char padding2[PADDING_BYTES];
public:
    ElapsedTimer() {
        calledStart = false;
    }
    void startTimer() {
        calledStart = true;
        start = std::chrono::high_resolution_clock::now();
    }
    int64_t getElapsedMicros() {
        if (!calledStart) {
            std::cout << "ERROR: called getElapsedMicros without calling startTimer\n";
            exit(1);
        }
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
    }
};

class PaddedRandom {
private:
    volatile char padding[PADDING_BYTES-sizeof(unsigned int)];
    unsigned int seed;
public:
    PaddedRandom(void) {
        this->seed = 0;
    }
    PaddedRandom(int seed) {
        this->seed = seed;
    }

    void setSeed(int seed) {
        this->seed = seed;
    }

    /** returns pseudorandom x satisfying 0 <= x < n. **/
    unsigned int nextNatural() {
        seed ^= seed << 6;
        seed ^= seed >> 21;
        seed ^= seed << 7;
        return seed;
    }
};

size_t getArrayletSize(size_t pagesize)
   {
    // 4096 * 16 * 16 = 1MB
    // 4096 * 16 = 64 KB
    return pagesize*ARRAYLET_SIZE_CONST; 
   }

void modifyContiguousMem(size_t pagesize, size_t arrayletSize, char * contiguousMap) 
{
    for(size_t i = 0; i < ARRAYLET_COUNT; i++) {
        /* Get the address representing the beginning of each arraylet */
        char *arrayletData = contiguousMap + (i * arrayletSize);

        /* write a pattern to the first page of each arraylet to verify proper mappings */
        memset(arrayletData, '*', 32);
        char *arrayletData2 = arrayletData + 48;
        memset(arrayletData2, '*', 16);

        /* Write to the first byte of each of the other pages in the arraylet to ensure all pages are touched */
        for (int j = 1; j < (arrayletSize / pagesize); j++) {
            char *pageData = arrayletData + (j * pagesize);
            *pageData = '*';
        }
    }
}

#ifdef LINUX_ARRAYLET
void freeAddresses(char * addresses[], size_t arrayletSize) 
   {
    for (size_t i = 0; i < ARRAYLET_COUNT+1; i++) {
        // std::cout << "Address[" << i << "]: " << (void *)addresses[i] << "\n";
        munmap(addresses[i], arrayletSize);
    }
   }
#endif

void freeAllocArray(void * allocArray)
   {
   free(allocArray);
   }

void printResults(size_t elapsedTime, size_t ignoreTotal, size_t avgPerIter, size_t avgIgnore)
   {
    std::cout << "Total time spent to create and modify both contiguous and heap locations: " 
              << elapsedTime << " microseconds (" << elapsedTime/1000000.0 << " seconds)" << "\n"; 
    std::cout << "Total time to free addresses (to ignore): " << ignoreTotal << " microseconds (" 
              << ignoreTotal/1000000.0 << " seconds)" << "\n"; 
    std::cout << "Total time - time ignored: " 
              << (elapsedTime - ignoreTotal) << " microseconds (" << (elapsedTime - ignoreTotal)/1000000.0 << " seconds)" << "\n"; 
    std::cout << "Average time per iteration - time ignored: " << (avgPerIter-avgIgnore) << " microseconds.\n";
    std::cout << "Average time to free addresses: " << avgIgnore << " microseconds.\n";
    std::cout << "Total Average iteration time: " << avgPerIter << " microseconds.\n";

    std::cout << "NOTE: 1 second = 10^6 microseconds.\n";
   }

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

#endif /* UTIL_H */
