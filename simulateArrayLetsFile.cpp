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

#define PADDING_BYTES 128
#define ARRAYLET_COUNT 5
#define FILE_COUNT 4

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

void dealocateArrayLets(char** arrayLets)
   {
   delete [] arrayLets;
   }

/**
 * Uses shm_open to create dummy empty file and
 * ftruncate to allocate desired memory size.
 */

int main(int argc, char** argv) {

    if (argc != 2) {
        std::cout<<"USAGE: " << argv[0] << " seed#" << std::endl;
        std::cout << "Example: " << argv[0] << " 6363" << std::endl;
        return 1;
    }

    PaddedRandom rnd;
    int seed = atoi(argv[1]);
    rnd.setSeed(seed);

    size_t pagesize = getpagesize(); // 4096 bytes
    std::cout << "System page size: " << pagesize << " bytes.\n";

    size_t mmapSize = pagesize;

    char ** arrayLets = getArrayLets(pagesize);
    std::cout << "ArrayLets addresses: " << '\n';
    for (size_t i = 0; i < ARRAYLET_COUNT; i++) {
      std::cout << "ArrayLet address " << i << ": " << (void*)arrayLets[i] << '\n';
      // std::cout << "ArrayLet " << i << ": " << arrayLets[i] << '\n';
    }

    // 1. Simulate the GC heap
    //     - Reserve heap range (256MB) maybe use mmap or ftruncate (combination?)
    //     - Commit memory
    //         - fd = shm_open(); ftruncate(fd, size)
    //         - mmap(NULL, size, PROT_NONE, MAP_SHARED, fd, 0)
    // 2. Simulate Arraylet
    //     - Grab random heap pagesize locations from step 1
    //     - Fill these pagesize chunks with numbers e.g. 11111111, 555555555
    // 3. Make Arraylets look contiguous with mmap
    //     - First mmap anonymously the total amount of arraylet size
    //       mapped = mmap(NULL, totalSize, PROT_READ)
    //     - For each arraylet map to "mapped" contiguous block

    // TODO: mmap to pre allocate memory space

    int fh1, fh2, fh3, fh4;
    fh1 = open("array1.txt", O_RDWR);
    fh2 = open("array2.txt", O_RDWR);
    fh3 = open("array3.txt", O_RDWR);
    fh4 = open("array4.txt", O_RDWR);

    if(fh1 < 0 || fh2 < 0 || fh3 < 0 || fh4 < 0) {
       std::cerr << "Error while reading files\n";
       return 1;
    }

    int arrFhs[FILE_COUNT] = {fh2, fh3, fh1, fh4};
    char * addresses[FILE_COUNT];

    int mapSize = pagesize*8;
    for (size_t i = 0; i < FILE_COUNT; i++) {

       addresses[i] = (char *)mmap(
                    (void*) (pagesize * (1 << 21) + (pagesize*(i*2))),
                    mapSize, // File size
                    PROT_READ|PROT_WRITE,
                    MAP_SHARED | MAP_FIXED,
                    arrFhs[i], // File handle
                    0);

       if (addresses[i] == MAP_FAILED) {
          std::cerr << "Failed to mmap index: " << i << "\n";
          return 1;
       } else {
         std::cout << "Successfully mmaped index " << i << " at address: " << (void *)addresses[i] << "\n";
       }

       mapSize -= pagesize*2;
    }

    close(fh1);
    close(fh2);
    close(fh3);
    close(fh4);

    std::cout << "mmapfh1 : " << addresses[0] << '\n'; // Base address, prints until \n

    std::cout << "mmapfh1[0][7000] : " << addresses[0][7000] << '\n';
    std::cout << "mmapfh1[0][9000] : " << addresses[0][9000] << '\n';
    std::cout << "mmapfh1[0][14000] : " << addresses[0][14000] << '\n';
    std::cout << "mmapfh1[0][20000] : " << addresses[0][20000] << '\n';
    std::cout << "mmapfh1[0][25000] : " << addresses[0][25000] << '\n';
    std::cout << "mmapfh1[0][32765] last actual char : " << addresses[0][32765] << '\n';
    std::cout << "mmapfh1[0][32766] last char (new line) : " << addresses[0][32766] << '\n';

    munmap(addresses[0], mapSize*FILE_COUNT);
    dealocateArrayLets(arrayLets);

    return 0;
}
