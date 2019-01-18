#ifndef UTIL_H
#define UTIL_H

#include <chrono>
#include <iostream>

#define PADDING_BYTES 128

class ElapsedTimer {
private:
    char padding0[PADDING_BYTES];
    bool calledStart = false;
    char padding1[PADDING_BYTES];
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
    char padding2[PADDING_BYTES];
public:
    void startTimer() {
        calledStart = true;
        start = std::chrono::high_resolution_clock::now();
    }
    int64_t getElapsedMillis() {
        if (!calledStart) {
            std::cout << "ERROR: called getElapsedMillis without calling startTimer\n";
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

#endif /* UTIL_H */