# include <fcntl.h>
# include <stdlib.h>
# include <sys/mman.h>
# include <string.h>
# include <math.h>
# include <assert.h>

#define PAGESIZE 4096
#define SEGARRAYSIZE 10

void *memAllocArray[SEGARRAYSIZE];
int numMallocs = 0;

typedef struct userMemHeader{
    size_t memSize;
    int freed; //1 - free 0 - in use
    struct pageHeader *headerPointer; //points to page Header
    struct userMemHeader *next; //points to next free block
    struct userMemHeader *prev;
} userMemHeader_t;

typedef struct pageHeader{
    size_t objSize;
    userMemHeader_t *freeList; 
    void *nextPage; //Pointer to next page
    void *nextFree; //Pointer to next spot to allocate
    int remainingSpots;
} pageHeader_t;

void *addPage(int index){
    //Grab New Page
    void *newPage = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    pageHeader_t *header = (pageHeader_t *)newPage;
    header -> freeList = NULL;
    header -> nextPage = NULL;
    header -> nextFree = header + 1;
    header -> remainingSpots = (PAGESIZE - sizeof(pageHeader_t)) / (sizeof(userMemHeader_t) + (1 << index));
    
    //initialize the page
    userMemHeader_t *userMemHeader = NULL;
    void *rover = header + 1;
    int i = 0;
    while (i < header -> remainingSpots){
        userMemHeader = rover;
        userMemHeader -> memSize = (1 << index);
        userMemHeader -> freed = 0;
        userMemHeader -> headerPointer = header;
        rover = rover + sizeof(userMemHeader_t) + (1 << index);
        if (i == header -> remainingSpots - 1) userMemHeader -> next = NULL;
        else userMemHeader -> next = rover;
        if (i == 0) userMemHeader -> prev = NULL;
        else userMemHeader -> prev = userMemHeader - (1 << index) - sizeof(userMemHeader_t);
        i++;
    }
    return newPage;
}

void *malloc(size_t size){
    if (size == 0) return NULL;

    //Initialize memAllocArray
    int i = 0;
    if (numMallocs == 0){
        while(i < SEGARRAYSIZE){
            memAllocArray[i] = addPage(i);
            i++;
        }
    }

    //Find the spot in memAllocList
    i = 1;
    while ((1 << i) < size && i <= SEGARRAYSIZE){
        i++;
    }

    //Allocate
    //Find page with available mem
    if (i <= SEGARRAYSIZE){
        pageHeader_t *header = memAllocArray[i - 1];
        while (header -> freeList == NULL || header -> remainingSpots == 0){
            if (header -> nextPage == NULL){
                void *newPage = addPage(i - 1);
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
        }
        numMallocs++;
        return userMemHeader + 1;
    }else{
        size_t numPages = (size + PAGESIZE - 1) / PAGESIZE;
        void *newPage = mmap(NULL, (numPages * PAGESIZE) + sizeof(pageHeader_t) + sizeof(userMemHeader_t), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        pageHeader_t *header = (pageHeader_t *)newPage;
        header->nextFree = header + 1;
        header->nextPage = NULL;
        header->freeList = NULL;
        header->remainingSpots = 0;
        header->objSize = size;

        userMemHeader_t *userMemHeader = (userMemHeader_t *)(header + 1);
        userMemHeader -> memSize = (1 << i);
        userMemHeader -> freed = 0;
        userMemHeader -> headerPointer = header;
        userMemHeader -> next = NULL;
        userMemHeader -> prev = NULL;
        numMallocs++;
        return header -> nextFree;
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
    userMemHeader_t *oldHeader = ptr - sizeof(userMemHeader_t);
    void *newLocation = malloc(size);
    memcpy(newLocation, ptr, oldHeader -> memSize);

    free(ptr);
    return newLocation;
}

void free(void *ptr){
    userMemHeader_t *ptrHeader = ptr - sizeof(userMemHeader_t);
    userMemHeader_t *rover = ptrHeader -> headerPointer -> freeList;

    while (rover -> next != NULL){
        rover = rover -> next;
    }

    rover -> next = ptrHeader;
    ptrHeader -> freed = 1;
}
