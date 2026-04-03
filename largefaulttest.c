/*
 * test_local.c  --  reproduces Speed Test 6 and Space Test 6 failures
 *
 * gcc -Wall -g test_local.c allocator.c -o test_local -ldl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern int large_path_count;

#define SMALL_ALLOCS   5000
#define MIXED_ITERS    10000

static long count_mapped_pages(void) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) { perror("fopen /proc/self/maps"); return -1; }

    long pages = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        unsigned long start, end;
        if (sscanf(line, "%lx-%lx", &start, &end) == 2)
            pages += (end - start) / 4096;
    }
    fclose(f);
    return pages;
}

static void test_space(void) {
    printf("=== SPACE TEST ===\n");
    printf("Pages before: %ld\n", count_mapped_pages());

    void *ptrs[SMALL_ALLOCS];
    for (int round = 0; round < 20; round++) {
        for (int i = 0; i < SMALL_ALLOCS; i++)
            ptrs[i] = malloc(64);
        for (int i = 0; i < SMALL_ALLOCS; i++)
            free(ptrs[i]);
    }

    long pages_after = count_mapped_pages();
    printf("Pages after 20 rounds of %d allocs: %ld\n", SMALL_ALLOCS, pages_after);
    printf("large_path_count: %d\n", large_path_count);
    if (pages_after > 60)
        printf("FAIL: too many pages (%ld > 60)\n", pages_after);
    else
        printf("PASS: page count looks reasonable\n");
}

static void test_speed(void) {
    printf("\n=== SPEED TEST ===\n");

    static const size_t sizes[] = { 16, 64, 256, 512, 2048, 8192, 65536 };
    int nsizes = sizeof(sizes) / sizeof(sizes[0]);

    void *ptrs[128];
    int nptrs = 0;

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    unsigned int seed = 42;
    for (int i = 0; i < MIXED_ITERS; i++) {
        int action = rand_r(&seed) % 3;
        if ((action < 2 || nptrs == 0) && nptrs < 128) {
            size_t sz = sizes[rand_r(&seed) % nsizes];
            void *p = malloc(sz);
            if (p) ptrs[nptrs++] = p;
        } else if (nptrs > 0) {
            int idx = rand_r(&seed) % nptrs;
            free(ptrs[idx]);
            ptrs[idx] = ptrs[--nptrs];
        }
    }
    for (int i = 0; i < nptrs; i++) free(ptrs[i]);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    printf("Elapsed: %.4f seconds for %d iterations\n", elapsed, MIXED_ITERS);
    if (elapsed > 5.0)
        printf("FAIL: would time out in autograder\n");
    else
        printf("PASS: fast enough\n");
}

static void test_page_leak(void) {
    printf("\n=== PAGE LEAK PROBE ===\n");
    long baseline = count_mapped_pages();
    long prev = baseline;

    for (int i = 1; i <= 10; i++) {
        void *p = malloc(64);
        free(p);
        long now = count_mapped_pages();
        printf("  iter %2d: pages = %ld (delta %+ld)\n", i, now, now - prev);
        prev = now;
    }

    long final = count_mapped_pages();
    if (final > baseline + 5)
        printf("FAIL: pages grew by %ld\n", final - baseline);
    else
        printf("PASS: no runaway page growth\n");
}

int main(void) {
    test_space();
    test_speed();
    test_page_leak();
    return 0;
}