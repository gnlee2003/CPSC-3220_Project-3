/*
 * test_local.c -- minimal malloc/free leak test, easy to step in gdb
 *
 * gcc -Wall -g test_local.c allocator.c -o test_local -ldl
 * gdb ./test_local
 */

#include <stdio.h>
#include <stdlib.h>

extern int large_path_count;  // add this to allocator.c at file scope

int main(void) {
    void *a = malloc(1024);
    printf("after malloc(64): large_path_count=%d\n", large_path_count);

    free(a);

    void *b = malloc(64);
    printf("after free+malloc(64): large_path_count=%d\n", large_path_count);

    free(b);
    return 0;
}