#ifndef SIMULATE_ARRAYLETS_HEAP
#define SIMULATE_ARRAYLETS_HEAP

#include <iostream>
#include <fstream>
#include <string>

#define LINUX_ARRAYLET

#include "util.hpp"

// To run:
// For MAC
// g++ -g3 -Wno-write-strings -std=c++11 simulateArrayLetsHeap.cpp -o simulateArrayLetsHeap
// For Linux with no c++11 support
// g++ -g3 -Wno-write-strings -std=c++0x simulateArrayLetsHeap.cpp -o simulateArrayLetsHeap
// Note: Insert -lrt flag for linux systems
// ./simulateArrayLetsHeap 12 1000 0

void
getAddressesOffset(int addressesCount, long *offsets)
{
   for(int i = 0; i < addressesCount; i++)
   {
       offsets[i] = i*i;
   }
}

char * mmapContiguous(size_t totalArraySize, size_t arrayletSize, long arrayLetOffsets[], int fh, int32_t flags)
   {
    // std::cout << "Calling mmapContiguous!!!\n"; 
    int mmapProt = 0;
	int mmapFlags = 0;

    mmapProt = PROT_READ | PROT_WRITE;
    if(flags & MMAP_FLAG_SHARED_ANON) {
        mmapFlags = MAP_SHARED | MAP_ANON;
    } else if(flags & MMAP_FLAG_PRIVATE_ANON) {
        mmapFlags = MAP_PRIVATE | MAP_ANON;
    } else {
        std::cerr << "Flags parameter not recognized.\n";
        return NULL;
    }
    char * contiguousMap = (char *)mmap(
                   NULL,
                   totalArraySize, // File size
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED | MAP_ANON, // Must be shared
                   -1,
                   0);

    if (contiguousMap == MAP_FAILED) {
      std::cerr << "Failed to mmap contiguousMap\n";
      return NULL;
    }

    mmapFlags = MAP_SHARED | MAP_FIXED;

    for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
       void *nextAddress = (void *)(contiguousMap+i*arrayletSize);
       void *address = mmap(
                   (void *)(contiguousMap+i*arrayletSize),
                   arrayletSize, // File size
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED | MAP_FIXED,
                   fh,
                   arrayLetOffsets[i]);

        if (address == MAP_FAILED) {
            std::cerr << "Failed to mmap address[" << i << "] at mmapContiguous(). errno: " << errno << "\n";
            munmap(contiguousMap, totalArraySize);
            contiguousMap = NULL;
        } else if (nextAddress != address) {
            std::cerr << "Map failed to provide the correct address. nextAddress " << nextAddress << " != " << address << std::endl;
            printf("Map failed to provide the correct address. nextAddress %p != %p\n", nextAddress, address);
            munmap(contiguousMap, totalArraySize);
            contiguousMap = NULL;
        }

        /*
         * @param: Address
         * @param: compile-time constant, 0 -> prepare for a read, 1 -> prepare for a write
         * @param: degree of temporal locality -> val between 0 - 3. Priority of which data needs to be left in cache. low - high
         */
         // __builtin_prefetch(address, 1 ,0);
    }

     return contiguousMap;
   }

/**
 * Uses shm_open to create dummy empty file and
 * ftruncate to allocate desired memory size.
 */

int main(int argc, char** argv) {

    if (argc != 4) {
        std::cout<<"USAGE: " << argv[0] << " seed# iterations# debug<0,1>" << std::endl;
        std::cout << "Example: " << argv[0] << " 6363 50000 0" << std::endl;
        return 1;
    }
    
    PaddedRandom rnd;
    int seed = atoi(argv[1]);
    int iterations = atoi(argv[2]);
    int debug = atoi(argv[3]);
    rnd.setSeed(seed);

    size_t pagesize = getpagesize(); // 4096 bytes
    std::cout << "System page size: " << pagesize << " bytes.\n";
    size_t arrayletSize = getArrayletSize(pagesize) * 4;
    std::cout << "Arraylet size: " << arrayletSize << " bytes" << std::endl;

	/*
	 * TODO: * Use std::tmpnam(nullptr) to create temporary file name
	 * 	   test with and without unlink to check if file is properly disposed/
	 * 		Answer: Documentation warns not to use it because of security risks. Up to 26 possible file names only
	 * 	 * Change shm_open to "open"
	 * 	 * Change shm_unlink to "unlink"
	 *	 	Answer: It does not work! Program hangs while trying to double map arraylets. 
	 * 	 * Check if heap mmap works with MAP_PRIVATE, using open and unlink
	 * 	 	Answer: It does work, however arraylet leaves must be mapped using MAP_SHARED 
	 * 		        (a lot faster than if they were MAP_PRIVATE)
	 * 	 * Chech mkstemp returns a file descriptor!
	 * 		Answer: same problem as open() (mkstemp calls open())
	 * 	 * Check memfd_create()
	 * 		Answer: sys/memfd.h not available
	 */

    int fileSize = 32;
    char filename[fileSize];
    // strcpy(filename, "tempfile0000000");
    int p = getpid();
    char int_str[fileSize/2 + 1];
    sprintf(filename, "tempfile%09d", p); // Max pid in 64bit system is 4194304
    std::cout << "Process ID: " << p << std::endl;
    // strcat(filename, int_str);
    // filename[fileSize - 1] = '\0';
    std::cout << "Filename generated: " << filename << std::endl;

    // std::cout << "pthread_self(): " << pthread_self() << std::endl;
    // std::cout << "(size_t)pthread_self(): " << (uint64_t)pthread_self() << std::endl;

    // int fh = shm_open(filename, O_RDWR | O_CREAT | O_EXCL, 0600);
    int fh = shm_open(filename, O_RDWR | O_CREAT, 0600);
    if (fh == -1) {
      std::cerr << "Error while reading file " << filename << "\n";
      return 1;
    }
    // shm_unlink(filename);
    shm_unlink(filename);

    // Sets the desired size to be allocated
    // Failing to allocate memory will result in a bus error on access.
    ftruncate(fh, FOUR_GB);

    int mmapProt = 0;
    int mmapFlags = 0;

    mmapProt = PROT_READ | PROT_WRITE;
    mmapFlags = MAP_SHARED;

    char * heapMmap = (char *)mmap(
                NULL,
                FOUR_GB, // File size
                mmapProt,
                mmapFlags, 
                fh, // File handle
                0);

    if (heapMmap == MAP_FAILED) {
       std::cerr << "Failed to mmap " << strerror(errno) << "\n";
       return 1;
    } else {
       std::cout << "Successfully mmaped heap at address: " << (void *)heapMmap << "\n";
    }

    // Get page alligned offsets
    long * arrayLetOffsets = (long *)malloc(ARRAYLET_COUNT * sizeof(long));
    for(size_t i = 0; i < ARRAYLET_COUNT; i++) {
        arrayLetOffsets[i] = getPageAlignedOffset(pagesize * 16, rnd.nextNatural() % FOUR_GB); // Change pagesize to match HUGETLB size
        std::cout << "Arralylet at " << i << " has offset: " << arrayLetOffsets[i] << std::endl;
    }
    
    char vals[SIXTEEN] = {'3', '5', '6', '8', '9', '0', '1', '2', '3', '7', 'A', 'E', 'C', 'B', 'D', 'F'};
    size_t totalArraySize = 0;

    std::cout << "************************************************\n";
    for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
        memset(heapMmap+arrayLetOffsets[i], vals[i%SIXTEEN], arrayletSize);
        totalArraySize += arrayletSize;
        void *baseAddress = heapMmap;
        void *arrayletAddress = heapMmap+arrayLetOffsets[i];
        long offset = (char *)arrayletAddress - (char *)baseAddress;
        std::cout << "Arralylet recheck " << i << " has offset: " << offset*sizeof(char) << std::endl;
    }
    std::cout << "************************************************\n";

    if (1 == debug) {
        fprintf(stdout, "First 32 chars of data before mapping and modification of the double mapped addresses\n");
        for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
            fprintf(stdout, "\tvals[%1lu] %.64s\n", i, heapMmap+arrayLetOffsets[i]);
        }
    }

    std::cout << "Arraylets created successfully.\n";
    std::cout << "ArrayLets combined have size: " << totalArraySize << " bytes." << '\n';

    double totalMapTime = 0;
    double totalModifyTime = 0;
    double totalFreeTime = 0;
    char * contiguous = NULL;

    ElapsedTimer timer;
    timer.startTimer();

    if (0 == debug) {

        for(size_t i = 0; i < iterations / 10; i++) {
            char *maps[10];
            for (int j = 0; j < 10; j++) {
                double start = timer.getElapsedMicros();
                // 3. Make Arraylets look contiguous with mmap
                maps[j] = mmapContiguous(totalArraySize, arrayletSize, arrayLetOffsets, fh, MMAP_FLAG_PRIVATE_ANON);

                double mapEnd = timer.getElapsedMicros();

                // 4. Modify contiguous memory view and observe change in the heap
                modifyContiguousMem(pagesize, arrayletSize, maps[j]);

                double modifyEnd = timer.getElapsedMicros();

                totalMapTime += (mapEnd - start);
                totalModifyTime += (modifyEnd - mapEnd);

            }
            for (int j = 0; j < 10; j++) {
                double freeStart = timer.getElapsedMicros();

                // Free addresses
                int ret = munmap(maps[j], totalArraySize);

                double freeEnd = timer.getElapsedMicros();

                totalFreeTime += (freeEnd - freeStart);
            }
        }
    } else {
        contiguous = mmapContiguous(totalArraySize, arrayletSize, arrayLetOffsets, fh, MMAP_FLAG_SHARED_ANON);
        fprintf(stdout, "Contiguous before modification!!!!!!!!!\n");
        for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
            char *arraylet = contiguous+(i*arrayletSize);
            fprintf(stdout, "\tcontiguous[%1lu] %.64s\n", i, arraylet);
        }
        modifyContiguousMem(pagesize, arrayletSize, contiguous);
        printf("\n");
    }

    int64_t elapsedTime = timer.getElapsedMicros();
    
    if (1 == debug) {
        fprintf(stdout, "First 32 chars of contiguous addresses\n");
        for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
            char *arraylet = contiguous+(i*arrayletSize);
            fprintf(stdout, "\tcontiguous[%1lu] %.64s\n", i, arraylet);
        }
        fprintf(stdout, "First 32 chars of data after mapping and modification of the double mapped addresses\n");
        for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
            char *arraylet = heapMmap+arrayLetOffsets[i];
            fprintf(stdout, "\theap[%1lu] %.64s\n", i, arraylet);
        }
    }

    std::cout << "Test completed " << iterations << " iterations" << std::endl;
    std::cout << "Total elapsed time " << elapsedTime << "us" << std::endl;
    std::cout << "Total map time " << totalMapTime << "us AVG map time " << (totalMapTime / iterations) << "us" << std::endl;
    std::cout << "Total modify time " << totalModifyTime << "us (" << (totalModifyTime/1000000) << "s) AVG modify time " << (totalModifyTime / iterations) << "us" << std::endl;
    std::cout << "Total free time " << totalFreeTime << "us (" << (totalFreeTime/1000000) << "s) AVG free time " << (totalFreeTime / iterations) << "us" << std::endl;

    munmap(heapMmap, FOUR_GB);

    return 0;
}

#endif /* SIMULATE_ARRAYLETS_HEAP */

