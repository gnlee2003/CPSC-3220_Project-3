#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define RESET "\033[0m"

#define PASS(msg) printf(GREEN "[PASS] " RESET "%s\n", msg)
#define FAIL(msg) printf(RED "[FAIL] " RESET "%s\n", msg)

// ---- Basic Tests ----

void test_malloc_basic(){
    void *ptr = malloc(8);
    if (ptr != NULL) PASS("malloc basic");
    else FAIL("malloc basic");
    free(ptr);
}

void test_malloc_zero(){
    void *ptr = malloc(0);
    printf("malloc(0) result: %p\n", ptr);
    if (ptr == NULL) PASS("malloc zero");
    else FAIL("malloc zero");
}

void test_calloc_zeroed(){
    int *ptr = calloc(10, sizeof(int));
    int allZero = 1;
    for (int i = 0; i < 10; i++){
        if (ptr[i] != 0){ allZero = 0; break; }
    }
    if (allZero) PASS("calloc zeroed");
    else FAIL("calloc zeroed");
    free(ptr);
}

void test_realloc_basic(){
    int *ptr = malloc(sizeof(int) * 4);
    if (ptr == NULL){ FAIL("realloc malloc failed"); return; }
    for (int i = 0; i < 4; i++) ptr[i] = i;
    
    int *newptr = realloc(ptr, sizeof(int) * 8);
    if (newptr == NULL){ FAIL("realloc returned NULL"); return; }
    
    if (newptr[0] == 0 && newptr[1] == 1 && newptr[2] == 2 && newptr[3] == 3)
        PASS("realloc preserves data");
    else{
        printf("  got: %d %d %d %d\n", newptr[0], newptr[1], newptr[2], newptr[3]);
        FAIL("realloc preserves data");
    }
    free(newptr);
}

void test_realloc_null(){
    void *ptr = realloc(NULL, 16);
    if (ptr != NULL) PASS("realloc NULL acts as malloc");
    else FAIL("realloc NULL acts as malloc");
    free(ptr);
}

// ---- Size Class Tests ----

void test_small_sizes(){
    int pass = 1;
    // test each size class boundary
    size_t sizes[] = {1, 2, 3, 4, 7, 8, 15, 16, 31, 32,
                      63, 64, 127, 128, 255, 256, 511, 512, 1023, 1024};
    for (int i = 0; i < 20; i++){
        void *ptr = malloc(sizes[i]);
        if (ptr == NULL){ pass = 0; break; }
        memset(ptr, 0xAB, sizes[i]);  // write to the memory
        free(ptr);
    }
    if (pass) PASS("small size classes");
    else FAIL("small size classes");
}

void test_large_alloc(){
    void *ptr = malloc(8192);
    if (ptr != NULL) PASS("large alloc");
    else FAIL("large alloc");
    memset(ptr, 0xAB, 8192);
    free(ptr);
}

void test_large_alloc_write(){
    int *ptr = malloc(sizeof(int) * 2048);
    int pass = 1;
    for (int i = 0; i < 2048; i++) ptr[i] = i;
    for (int i = 0; i < 2048; i++){
        if (ptr[i] != i){ pass = 0; break; }
    }
    if (pass) PASS("large alloc write/read");
    else FAIL("large alloc write/read");
    free(ptr);
}

// ---- Free List Tests ----

void test_free_reuse(){
    void *ptr1 = malloc(8);
    free(ptr1);
    void *ptr2 = malloc(8);
    // freed block should be reused
    if (ptr2 == ptr1) PASS("free reuse");
    else FAIL("free reuse");
    free(ptr2);
}

void test_multiple_alloc_free(){
    int pass = 1;
    void *ptrs[100];
    for (int i = 0; i < 100; i++){
        ptrs[i] = malloc(16);
        if (ptrs[i] == NULL){ pass = 0; break; }
        memset(ptrs[i], i, 16);
    }
    for (int i = 0; i < 100; i++) free(ptrs[i]);
    if (pass) PASS("multiple alloc/free");
    else FAIL("multiple alloc/free");
}

// ---- Page Boundary Tests ----

void test_fill_page(){
    // allocate enough 8-byte blocks to fill a page
    // page = 4096, header ~= 40 bytes, each slot = 8 + userMemHeader
    int pass = 1;
    void *ptrs[200];
    for (int i = 0; i < 200; i++){
        ptrs[i] = malloc(8);
        if (ptrs[i] == NULL){ pass = 0; break; }
        memset(ptrs[i], 0xCD, 8);
    }
    for (int i = 0; i < 200; i++){
        if (ptrs[i] != NULL) free(ptrs[i]);
    }
    if (pass) PASS("fill page triggers new page");
    else FAIL("fill page triggers new page");
}

// ---- Overlap Tests ----

void test_no_overlap(){
    int pass = 1;
    void *ptrs[50];
    for (int i = 0; i < 50; i++){
        ptrs[i] = malloc(64);
        // write unique pattern
        memset(ptrs[i], i, 64);
    }
    // verify no block was overwritten
    for (int i = 0; i < 50; i++){
        unsigned char *p = ptrs[i];
        for (int j = 0; j < 64; j++){
            if (p[j] != (unsigned char)i){ pass = 0; break; }
        }
    }
    for (int i = 0; i < 50; i++) free(ptrs[i]);
    if (pass) PASS("no overlap between allocations");
    else FAIL("no overlap between allocations");
}

// ---- Random Test ----

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
    for (int i = 0; i < 200; i++){
        if (ptrs[i] != NULL) free(ptrs[i]);
    }
    if (pass) PASS("random alloc/free");
    else FAIL("random alloc/free");
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
    test_large_alloc_write();

    printf("\n=== Free List Tests ===\n");
    test_free_reuse();
    test_multiple_alloc_free();

    printf("\n=== Page Boundary Tests ===\n");
    test_fill_page();

    printf("\n=== Overlap Tests ===\n");
    test_no_overlap();

    printf("\n=== Random Tests ===\n");
    test_random();

    return 0;
}