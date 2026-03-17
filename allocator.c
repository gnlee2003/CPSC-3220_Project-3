# include <fcntl.h>
# include <stdlib.h>
# include <sys/mman.h>
# include <string.h>
# include <assert.h>
# include <unistd.h>
# include <stdio.h>

#define PAGESIZE 4096
#define SEGARRAYSIZE 10
#define MAXARRAYSIZE 1024

void *memAllocArray[SEGARRAYSIZE];
int initialize = 0; //if 0 initialize the array

typedef struct userMemHeader{
    size_t memSize;
    int freed; //1 - free 0 - in use
    struct pageHeader *headerPointer; //points to page Header
    struct userMemHeader *next; //points to next free block
} userMemHeader_t;

typedef struct pageHeader{
    size_t objSize;
    userMemHeader_t *freeList; 
    void *nextPage; //Pointer to next page
    void *nextFree; //Pointer to next spot to allocate
    int remainingSpots;
} pageHeader_t;

void *addPage(int index){
    index++;
    //Grab New Page
    void *newPage = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    pageHeader_t *header = (pageHeader_t *)newPage;
    header -> freeList = NULL;
    header -> nextPage = NULL;
    header -> nextFree = (char *)header + sizeof(pageHeader_t);
    header -> remainingSpots = (PAGESIZE - sizeof(pageHeader_t)) / (sizeof(userMemHeader_t) + (1 << index));
    
    //initialize the page
    userMemHeader_t *userMemHeader = NULL;
    void *rover = (char *)header + sizeof(pageHeader_t);
    int i = 0;

    while (i < header -> remainingSpots){
        userMemHeader = rover;
        userMemHeader -> memSize = (1 << index);
        userMemHeader -> freed = 0;
        userMemHeader -> headerPointer = header;
        rover = (char *)rover + sizeof(userMemHeader_t) + (1 << index);
        if (i == header -> remainingSpots - 1) userMemHeader -> next = NULL;
        else userMemHeader -> next = rover;
        i++;
    }
    return newPage;
}

void *malloc(size_t size){
    if (size == 0) return NULL;
    int power = 1, index = 0;

    //Initialize memAllocArray
    if (initialize == 0){
        while(index < SEGARRAYSIZE){
            memAllocArray[index] = addPage(index);
            index++;
        }
        initialize++;
    }

    //Find the spot in memAllocList
    while ((1 << power) < size && power < SEGARRAYSIZE){
        power++;
    }

    //Allocate
    index = power - 1;
    //Find page with available mem
    if (power < SEGARRAYSIZE){
        pageHeader_t *header = memAllocArray[index];
        while (header -> freeList == NULL && header -> remainingSpots == 0){
            if (header -> nextPage == NULL){
                void *newPage = addPage(index);
                header -> nextPage = newPage;
            }
            header = header -> nextPage;
        }

        //find the correct mem location
        userMemHeader_t *userMemHeader = NULL;
        assert(header -> freeList != NULL || header -> remainingSpots != 0);
        if (header -> freeList != NULL){
            userMemHeader = header -> freeList;
            header -> freeList = userMemHeader -> next;
        }else if (header -> remainingSpots != 0){
            userMemHeader = header -> nextFree;
            header -> nextFree = userMemHeader -> next;
            header -> remainingSpots--;
        }
        return (char *)userMemHeader + sizeof(userMemHeader_t);
    }else{
        size_t numPages = (size + PAGESIZE - 1) / PAGESIZE;
        void *newPage = mmap(NULL, (numPages * PAGESIZE) + sizeof(pageHeader_t) + sizeof(userMemHeader_t), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        pageHeader_t *header = (pageHeader_t *)newPage;
        header->nextFree = (char *)header + sizeof(pageHeader_t);
        header->nextPage = NULL;
        header->freeList = NULL;
        header->remainingSpots = 0;
        header->objSize = size;

        userMemHeader_t *userMemHeader = (userMemHeader_t *)((char *)header + sizeof(pageHeader_t));
        userMemHeader -> memSize = size;
        userMemHeader -> freed = 0;
        userMemHeader -> headerPointer = header;
        userMemHeader -> next = NULL;
        return (void *)((char *)header + sizeof(pageHeader_t) + sizeof(userMemHeader_t));
    }
    return NULL;
}

void *calloc(size_t nmemb, size_t size){
    //get the mem
    void *mem = malloc(nmemb * size);

    //Set mem = 0
    if (mem == NULL) return NULL;
    memset(mem, 0, nmemb * size);
    return mem;
}

void *realloc(void *ptr, size_t size){
    if (ptr == NULL) return malloc(size);
    if (size == 0){
        free(ptr);
        return NULL;
    }

    userMemHeader_t *oldHeader = (userMemHeader_t *)((char *)ptr - sizeof(userMemHeader_t));
    void *newLocation = malloc(size);
    if (newLocation == NULL) return NULL;
    size_t copySize = oldHeader->memSize < size ? oldHeader->memSize : size;
    memcpy(newLocation, ptr, copySize);

    free(ptr);
    return newLocation;
}

void free(void *ptr){
    if (ptr == NULL) return;
    
    userMemHeader_t *ptrHeader = (userMemHeader_t *)((char *)ptr - sizeof(userMemHeader_t));
    pageHeader_t *pageHeader = ptrHeader -> headerPointer;

    if (pageHeader -> objSize > MAXARRAYSIZE){
        munmap(pageHeader, pageHeader -> objSize + sizeof(pageHeader) + sizeof(userMemHeader_t));
    }else{
        ptrHeader -> freed = 1;
        ptrHeader -> next = pageHeader -> freeList;
        pageHeader -> freeList = ptrHeader;
    }
}
