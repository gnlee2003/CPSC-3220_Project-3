# include <fcntl.h>
# include <stdlib.h>
# include <sys/mman.h>
# include <string.h>
# include <assert.h>
# include <unistd.h>
# include <stdio.h>


void test_realloc_comprehensive(){
    printf("=== Realloc Tests ===\n");

    // Test 1: basic grow
    int *ptr = malloc(sizeof(int) * 4);
    for (int i = 0; i < 4; i++) ptr[i] = i;
    ptr = realloc(ptr, sizeof(int) * 8);
    int pass = 1;
    for (int i = 0; i < 4; i++) if (ptr[i] != i) pass = 0;
    if (pass) printf("[PASS] realloc grow preserves data\n");
    else printf("[FAIL] realloc grow preserves data\n");
    free(ptr);

    // Test 2: shrink
    ptr = malloc(sizeof(int) * 8);
    for (int i = 0; i < 8; i++) ptr[i] = i;
    ptr = realloc(ptr, sizeof(int) * 4);
    pass = 1;
    for (int i = 0; i < 4; i++) if (ptr[i] != i) pass = 0;
    if (pass) printf("[PASS] realloc shrink preserves data\n");
    else printf("[FAIL] realloc shrink preserves data\n");
    free(ptr);

    // Test 3: realloc NULL acts as malloc
    ptr = realloc(NULL, sizeof(int) * 4);
    if (ptr != NULL) printf("[PASS] realloc NULL\n");
    else printf("[FAIL] realloc NULL\n");
    free(ptr);

    // Test 4: realloc to 0 acts as free
    ptr = malloc(sizeof(int) * 4);
    ptr = realloc(ptr, 0);
    if (ptr == NULL) printf("[PASS] realloc size 0\n");
    else printf("[FAIL] realloc size 0\n");

    // Test 5: grow across size classes
    char *cptr = malloc(8);
    memset(cptr, 'A', 8);
    cptr = realloc(cptr, 64);
    pass = 1;
    for (int i = 0; i < 8; i++) if (cptr[i] != 'A') pass = 0;
    if (pass) printf("[PASS] realloc grow across size class\n");
    else printf("[FAIL] realloc grow across size class\n");
    free(cptr);

    // Test 6: grow from small to large
    cptr = malloc(64);
    memset(cptr, 'B', 64);
    cptr = realloc(cptr, 8192);
    pass = 1;
    for (int i = 0; i < 64; i++) if (cptr[i] != 'B') pass = 0;
    if (pass) printf("[PASS] realloc small to large\n");
    else printf("[FAIL] realloc small to large\n");
    free(cptr);

    // Test 7: grow large to larger
    cptr = malloc(4096);
    memset(cptr, 'C', 4096);
    cptr = realloc(cptr, 8192);
    pass = 1;
    for (int i = 0; i < 4096; i++) if (cptr[i] != 'C') pass = 0;
    if (pass) printf("[PASS] realloc large to larger\n");
    else printf("[FAIL] realloc large to larger\n");
    free(cptr);

    // Test 8: multiple reallocs
    ptr = malloc(sizeof(int));
    ptr[0] = 42;
    for (int n = 2; n <= 128; n *= 2){
        ptr = realloc(ptr, sizeof(int) * n);
        if (ptr == NULL){ printf("[FAIL] realloc multiple NULL\n"); return; }
        ptr[n - 1] = n;
    }
    if (ptr[0] == 42) printf("[PASS] realloc multiple preserves first element\n");
    else printf("[FAIL] realloc multiple preserves first element\n");
    free(ptr);

    // Test 9: realloc same size
    ptr = malloc(sizeof(int) * 4);
    for (int i = 0; i < 4; i++) ptr[i] = i * 10;
    ptr = realloc(ptr, sizeof(int) * 4);
    pass = 1;
    for (int i = 0; i < 4; i++) if (ptr[i] != i * 10) pass = 0;
    if (pass) printf("[PASS] realloc same size\n");
    else printf("[FAIL] realloc same size\n");
    free(ptr);

    // Test 10: write after realloc
    cptr = malloc(16);
    cptr = realloc(cptr, 256);
    memset(cptr, 0xAB, 256);
    pass = 1;
    for (int i = 0; i < 256; i++) if ((unsigned char)cptr[i] != 0xAB) pass = 0;
    if (pass) printf("[PASS] realloc write after grow\n");
    else printf("[FAIL] realloc write after grow\n");
    free(cptr);
}

int main(){
    test_realloc_comprehensive();
    return 0;
}