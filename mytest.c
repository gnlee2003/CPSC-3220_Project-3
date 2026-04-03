#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

int main(){
    write(1, "Malloc\n", sizeof("Malloc\n"));
    void *ptr = malloc(8);
    //printf("free\n");
    free(ptr);
}