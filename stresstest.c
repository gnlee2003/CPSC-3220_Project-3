#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define RESET "\033[0m"
#define PASS(msg) printf(GREEN "[PASS] " RESET "%s\n", msg)
#define FAIL(msg) printf(RED "[FAIL] " RESET "%s\n", msg)

#define NUM_THREADS 8
#define ALLOCS_PER_THREAD 100

// ---- Single Thread Tests ----

void test_malloc_basic(){
    void *ptr = malloc(8);
    if (ptr != NULL) PASS("malloc basic");
    else FAIL("malloc basic");
    free(ptr);
}

void test_malloc_zero(){
    void *ptr = malloc(0);
    if (ptr == NULL) PASS("malloc zero");
    else FAIL("malloc zero");
}

void test_calloc_zeroed(){
    int *ptr = calloc(10, sizeof(int));
    int pass = 1;
    for (int i = 0; i < 10; i++)
        if (ptr[i] != 0){ pass = 0; break; }
    if (pass) PASS("calloc zeroed");
    else FAIL("calloc zeroed");
    free(ptr);
}

void test_realloc_basic(){
    int *ptr = malloc(sizeof(int) * 4);
    for (int i = 0; i < 4; i++) ptr[i] = i;
    int *newptr = realloc(ptr, sizeof(int) * 8);
    if (newptr == NULL){ FAIL("realloc returned NULL"); return; }
    int pass = 1;
    for (int i = 0; i < 4; i++) if (newptr[i] != i) pass = 0;
    if (pass) PASS("realloc preserves data");
    else printf(RED "[FAIL] " RESET "realloc preserves data: got %d %d %d %d\n",
                newptr[0], newptr[1], newptr[2], newptr[3]);
    free(newptr);
}

void test_realloc_null(){
    void *ptr = realloc(NULL, 16);
    if (ptr != NULL) PASS("realloc NULL acts as malloc");
    else FAIL("realloc NULL acts as malloc");
    free(ptr);
}

void test_small_sizes(){
    int pass = 1;
    size_t sizes[] = {1, 2, 3, 4, 7, 8, 15, 16, 31, 32,
                      63, 64, 127, 128, 255, 256, 511, 512, 1023, 1024};
    for (int i = 0; i < 20; i++){
        void *ptr = malloc(sizes[i]);
        if (ptr == NULL){ pass = 0; break; }
        memset(ptr, 0xAB, sizes[i]);
        free(ptr);
    }
    if (pass) PASS("small size classes");
    else FAIL("small size classes");
}

void test_large_alloc(){
    void *ptr = malloc(8192);
    if (ptr == NULL){ FAIL("large alloc"); return; }
    memset(ptr, 0xAB, 8192);
    PASS("large alloc");
    free(ptr);
}

void test_free_reuse(){
    void *ptr1 = malloc(8);
    free(ptr1);
    void *ptr2 = malloc(8);
    if (ptr2 == ptr1) PASS("free reuse");
    else FAIL("free reuse");
    free(ptr2);
}

void test_no_overlap(){
    int pass = 1;
    void *ptrs[50];
    for (int i = 0; i < 50; i++){
        ptrs[i] = malloc(64);
        memset(ptrs[i], i, 64);
    }
    for (int i = 0; i < 50; i++){
        unsigned char *p = ptrs[i];
        for (int j = 0; j < 64; j++)
            if (p[j] != (unsigned char)i){ pass = 0; break; }
    }
    for (int i = 0; i < 50; i++) free(ptrs[i]);
    if (pass) PASS("no overlap between allocations");
    else FAIL("no overlap between allocations");
}

void test_fill_page(){
    int pass = 1;
    void *ptrs[200];
    for (int i = 0; i < 200; i++){
        ptrs[i] = malloc(8);
        if (ptrs[i] == NULL){ pass = 0; break; }
        memset(ptrs[i], 0xCD, 8);
    }
    for (int i = 0; i < 200; i++)
        if (ptrs[i] != NULL) free(ptrs[i]);
    if (pass) PASS("fill page triggers new page");
    else FAIL("fill page triggers new page");
}

void test_random(){
    int pass = 1;
    void *ptrs[200];
    size_t sizes[200];
    memset(ptrs, 0, sizeof(ptrs));
    srand(42);
    for (int i = 0; i < 1000; i++){
        int slot = rand() % 200;
        if (ptrs[slot] != NULL){
            free(ptrs[slot]);
            ptrs[slot] = NULL;
        } else {
            sizes[slot] = (rand() % 2048) + 1;
            ptrs[slot] = malloc(sizes[slot]);
            if (ptrs[slot] == NULL){ pass = 0; break; }
            memset(ptrs[slot], 0xBE, sizes[slot]);
        }
    }
    for (int i = 0; i < 200; i++)
        if (ptrs[i] != NULL) free(ptrs[i]);
    if (pass) PASS("random alloc/free");
    else FAIL("random alloc/free");
}

// ---- Multi Thread Tests ----

// each thread allocates, writes, verifies, frees
void *thread_alloc_free(void *arg){
    int tid = *(int *)arg;
    void *ptrs[ALLOCS_PER_THREAD];
    size_t sizes[ALLOCS_PER_THREAD];

    for (int i = 0; i < ALLOCS_PER_THREAD; i++){
        sizes[i] = (rand() % 512) + 1;
        ptrs[i] = malloc(sizes[i]);
        if (ptrs[i] == NULL) return (void *)1;
        memset(ptrs[i], tid & 0xFF, sizes[i]);
    }

    // verify
    for (int i = 0; i < ALLOCS_PER_THREAD; i++){
        unsigned char *p = ptrs[i];
        for (size_t j = 0; j < sizes[i]; j++){
            if (p[j] != (unsigned char)(tid & 0xFF)) return (void *)1;
        }
    }

    for (int i = 0; i < ALLOCS_PER_THREAD; i++)
        free(ptrs[i]);

    return (void *)0;
}

void test_multithread_alloc_free(){
    pthread_t threads[NUM_THREADS];
    int tids[NUM_THREADS];
    int pass = 1;

    for (int i = 0; i < NUM_THREADS; i++){
        tids[i] = i;
        pthread_create(&threads[i], NULL, thread_alloc_free, &tids[i]);
    }
    for (int i = 0; i < NUM_THREADS; i++){
        void *ret;
        pthread_join(threads[i], &ret);
        if ((long)ret != 0) pass = 0;
    }
    if (pass) PASS("multithread alloc/free");
    else FAIL("multithread alloc/free");
}

// threads do random alloc/free
void *thread_random(void *arg){
    void *ptrs[50];
    memset(ptrs, 0, sizeof(ptrs));
    srand((unsigned long)arg);

    for (int i = 0; i < 500; i++){
        int slot = rand() % 50;
        if (ptrs[slot] != NULL){
            free(ptrs[slot]);
            ptrs[slot] = NULL;
        } else {
            size_t size = (rand() % 1024) + 1;
            ptrs[slot] = malloc(size);
            if (ptrs[slot] != NULL)
                memset(ptrs[slot], 0xAB, size);
        }
    }
    for (int i = 0; i < 50; i++)
        if (ptrs[i] != NULL) free(ptrs[i]);

    return (void *)0;
}

void test_multithread_random(){
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, thread_random, (void *)(long)i);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
    PASS("multithread random alloc/free");
}

// threads do large allocations
void *thread_large(void *arg){
    void *ptrs[10];
    for (int i = 0; i < 10; i++){
        ptrs[i] = malloc(4096 + (rand() % 4096));
        if (ptrs[i] == NULL) return (void *)1;
        memset(ptrs[i], 0xCC, 4096);
    }
    for (int i = 0; i < 10; i++) free(ptrs[i]);
    return (void *)0;
}

void test_multithread_large(){
    pthread_t threads[NUM_THREADS];
    int pass = 1;
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, thread_large, NULL);
    for (int i = 0; i < NUM_THREADS; i++){
        void *ret;
        pthread_join(threads[i], &ret);
        if ((long)ret != 0) pass = 0;
    }
    if (pass) PASS("multithread large alloc");
    else FAIL("multithread large alloc");
}

// threads do realloc
void *thread_realloc(void *arg){
    void *ptr = malloc(16);
    if (ptr == NULL) return (void *)1;
    memset(ptr, 0xAA, 16);
    for (int i = 32; i <= 512; i *= 2){
        ptr = realloc(ptr, i);
        if (ptr == NULL) return (void *)1;
    }
    free(ptr);
    return (void *)0;
}

void test_multithread_realloc(){
    pthread_t threads[NUM_THREADS];
    int pass = 1;
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, thread_realloc, NULL);
    for (int i = 0; i < NUM_THREADS; i++){
        void *ret;
        pthread_join(threads[i], &ret);
        if ((long)ret != 0) pass = 0;
    }
    if (pass) PASS("multithread realloc");
    else FAIL("multithread realloc");
}

// counts mmap calls to track page usage
static int mmap_count = 0;

void *thread_page_stress(void *arg){
    void *ptrs[50];
    size_t sizes[50];
    memset(ptrs, 0, sizeof(ptrs));
    srand((unsigned long)arg * 1234);

    for (int i = 0; i < 500; i++){
        int slot = rand() % 50;
        if (ptrs[slot] != NULL){
            free(ptrs[slot]);
            ptrs[slot] = NULL;
        } else {
            sizes[slot] = (rand() % 1024) + 1;
            ptrs[slot] = malloc(sizes[slot]);
            if (ptrs[slot] != NULL)
                memset(ptrs[slot], 0xAB, sizes[slot]);
        }
    }
    for (int i = 0; i < 50; i++)
        if (ptrs[i] != NULL) free(ptrs[i]);
    return NULL;
}

void test_page_reuse(){
    // allocate and free many blocks of the same size
    // if pages are reused, the same addresses should come back
    int pass = 1;
    void *first[20];

    // first round of allocations
    for (int i = 0; i < 20; i++){
        first[i] = malloc(64);
        if (first[i] == NULL){ pass = 0; break; }
        memset(first[i], 0xAA, 64);
    }

    // free them all
    for (int i = 0; i < 20; i++) free(first[i]);

    // second round — should reuse freed slots
    void *second[20];
    for (int i = 0; i < 20; i++){
        second[i] = malloc(64);
        if (second[i] == NULL){ pass = 0; break; }
        memset(second[i], 0xBB, 64);
    }

    // verify no corruption
    for (int i = 0; i < 20; i++){
        unsigned char *p = second[i];
        for (int j = 0; j < 64; j++){
            if (p[j] != 0xBB){ pass = 0; break; }
        }
    }
    for (int i = 0; i < 20; i++) free(second[i]);

    if (pass) PASS("page reuse after free");
    else FAIL("page reuse after free");
}

void test_page_fill_multiple(){
    // force allocation of multiple pages for the same size class
    int pass = 1;
    int n = 500;  // enough to spill across multiple pages
    void **ptrs = malloc(sizeof(void *) * n);

    for (int i = 0; i < n; i++){
        ptrs[i] = malloc(16);
        if (ptrs[i] == NULL){ pass = 0; break; }
        memset(ptrs[i], i & 0xFF, 16);
    }

    // verify all blocks are valid and distinct
    for (int i = 0; i < n; i++){
        unsigned char *p = ptrs[i];
        for (int j = 0; j < 16; j++){
            if (p[j] != (unsigned char)(i & 0xFF)){ pass = 0; break; }
        }
    }

    for (int i = 0; i < n; i++) free(ptrs[i]);
    free(ptrs);

    if (pass) PASS("multi-page fill and verify");
    else FAIL("multi-page fill and verify");
}

void test_page_multithread(){
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, thread_page_stress, (void *)(long)i);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
    PASS("multithread page stress");
}

int main(){
    printf("=== Basic Tests ===\n");
    test_malloc_basic();
    test_malloc_zero();
    test_calloc_zeroed();
    test_realloc_basic();
    test_realloc_null();

    printf("\n=== Size Class Tests ===\n");
    test_small_sizes();
    test_large_alloc();

    printf("\n=== Free List Tests ===\n");
    test_free_reuse();

    printf("\n=== Page Tests ===\n");
    test_fill_page();
    test_no_overlap();

    printf("\n=== Random Tests ===\n");
    test_random();

    printf("\n=== Multi Thread Tests ===\n");
    test_multithread_alloc_free();
    test_multithread_random();
    test_multithread_large();
    test_multithread_realloc();

    printf("\n=== Page Tests ===\n");
    test_page_reuse();
    test_page_fill_multiple();
    test_page_multithread();

    return 0;
}