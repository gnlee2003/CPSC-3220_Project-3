# include <fcntl.h>
# include <stdlib.h>
# include <sys/mman.h>
# include <string.h>
# include <unistd.h>
# include <stdio.h>
# include <pthread.h>

#define PAGESIZE 4096
#define SEGARRAYSIZE 10
#define MAXARRAYSIZE 1024

void *memAllocArray[SEGARRAYSIZE]; //Array to hold segregated free list
int initialize = 0; //if 0 initialize the array
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; //Lock for thread Safety

//Structure used to house the header for each block of memory given to the user
typedef struct userMemHeader{
    size_t memSize; //Size of the given block
    int freed; //1 - free 0 - in use
    struct pageHeader *headerPointer; //points to page Header
    struct userMemHeader *next; //points to next free block
} userMemHeader_t;

//Structure used to house the header for each page of memory pulled from mmap
typedef struct pageHeader{
    size_t objSize;
    userMemHeader_t *freeList; 
    void *nextPage; //Pointer to next page
    void *nextFree; //Pointer to next spot to allocate
    int remainingSpots;
} pageHeader_t;

//This function grabs a page using mmap and initializes its memory for use
//This function is only used for adding pages to the segregated free list
void *addPage(int index){
    //Grab New Page
    void *newPage = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    //Add the page Header to the new page
    pageHeader_t *header = (pageHeader_t *)newPage;
    header -> freeList = NULL;
    header -> nextPage = NULL;
    header -> nextFree = (char *)header + sizeof(pageHeader_t);
    header -> remainingSpots = (PAGESIZE - sizeof(pageHeader_t)) / (sizeof(userMemHeader_t) + (1 << index));
    
    //initialize the page with the blocks of user memory
    userMemHeader_t *userMemHeader = NULL;
    void *rover = (char *)header + sizeof(pageHeader_t);
    int i = 0;

    while (i < header -> remainingSpots){
        userMemHeader = rover;
        userMemHeader -> memSize = (1 << index);
        userMemHeader -> freed = 0;
        userMemHeader -> headerPointer = header;
        rover = (char *)rover + sizeof(userMemHeader_t) + (1 << index);
        if (i == header -> remainingSpots - 1) userMemHeader -> next = NULL; //Make sure that there is enough space for another block
        else userMemHeader -> next = rover;
        i++;
    }
    return newPage;
}

//malloc takes a size in bytes and returns a pointer to memory for the caller to use
//malloc returns NULL when size is 0
void *malloc(size_t size){
    pthread_mutex_lock(&lock); //Lock for thread safety

    if (size == 0){
        pthread_mutex_unlock(&lock);
        return NULL;
    }
    int power = 1, index = 0;

    //Initialize the segregated free list on first call to malloc
    //Gives one free page memory
    if (initialize == 0){
        while(index < SEGARRAYSIZE){
            memAllocArray[index] = addPage(index + 1);
            index++;
        }
        initialize++;
    }

    //Find the spot in memAllocList
    while ((1 << power) < size && power <= SEGARRAYSIZE){
        power++;
    }

    //Allocate
    index = power - 1;
    //Find page with available mem
    if (power <= SEGARRAYSIZE){
        pageHeader_t *header = memAllocArray[index];
        while (header -> freeList == NULL && header -> remainingSpots == 0){ //First search for the correct page in the free list
            if (header -> nextPage == NULL){ //If we have reached the end of the available spots in the pages, add a new page
                void *newPage = addPage(index + 1);
                header -> nextPage = newPage;
            }
            header = header -> nextPage;
        }

        //find the correct mem location
        userMemHeader_t *userMemHeader = NULL;
        if (header -> freeList != NULL){
            userMemHeader = header -> freeList;
            header -> freeList = userMemHeader -> next;
        }else if (header -> remainingSpots != 0){
            userMemHeader = header -> nextFree;
            header -> nextFree = userMemHeader -> next;
            header -> remainingSpots--;
        }
        pthread_mutex_unlock(&lock); //Unlock
        return (char *)userMemHeader + sizeof(userMemHeader_t);
    }else{ //If this is a large allocation
        void *newPage = mmap(NULL, size + sizeof(pageHeader_t) + sizeof(userMemHeader_t), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        
        //Create and initialize the page header
        pageHeader_t *header = (pageHeader_t *)newPage;
        header->nextFree = (char *)header + sizeof(pageHeader_t);
        header->nextPage = NULL;
        header->freeList = NULL;
        header->remainingSpots = 0;
        header->objSize = size;

        //Create and initialize the user block header
        userMemHeader_t *userMemHeader = (userMemHeader_t *)((char *)header + sizeof(pageHeader_t));
        userMemHeader -> memSize = size;
        userMemHeader -> freed = 0;
        userMemHeader -> headerPointer = header;
        userMemHeader -> next = NULL;
        pthread_mutex_unlock(&lock); //Unlock
        return (void *)((char *)header + sizeof(pageHeader_t) + sizeof(userMemHeader_t));
    }
    return NULL;
}

//calloc takes two arguments, the number of elements and the size of each element and setting the values of the memory to 0
//This calloc works by simply calling malloc with nmemb * size and memset to set the values inside the given memory to 0
//Does not need a lock for thread safe due to only using malloc and free, which are thread safe
void *calloc(size_t nmemb, size_t size){
    //get the mem
    void *mem = malloc(nmemb * size);

    //Set mem = 0
    if (mem == NULL) return NULL;
    memset(mem, 0, nmemb * size);
    return mem;
}

//realloc takes 2 arguments, one is an old ptr to an old block of memory and the other is a new size for the memory
//realloc resizes the given ptr to a new one using malloc, free, and memcpy
//Does not need a lock for thread safe due to only using malloc and free, which are thread safe
void *realloc(void *ptr, size_t size){
    //Edge cases
    if (ptr == NULL) return malloc(size);
    if (size == 0){
        free(ptr);
        return NULL;
    }


    userMemHeader_t *oldHeader = (userMemHeader_t *)((char *)ptr - sizeof(userMemHeader_t));
    void *newLocation = malloc(size);
    if (newLocation == NULL) return NULL;
    size_t copySize = oldHeader->memSize < size ? oldHeader->memSize : size; //Get correct size to be copied
    memcpy(newLocation, ptr, copySize);

    free(ptr);
    return newLocation;
}

//takes an old ptr and gives the memory back to the kernel
void free(void *ptr){
    pthread_mutex_lock(&lock); //Lock for thread safety

    if (ptr == NULL){
        pthread_mutex_unlock(&lock);
        return;
    }
    
    userMemHeader_t *ptrHeader = (userMemHeader_t *)((char *)ptr - sizeof(userMemHeader_t)); //Find the user memory header Header
    pageHeader_t *pageHeader = ptrHeader -> headerPointer; //Find the page header

    if (pageHeader->objSize > MAXARRAYSIZE){ //If this ptr is a large allocation
        //Calculate the size
        size_t totalSize = pageHeader -> objSize + sizeof(pageHeader_t) + sizeof(userMemHeader_t);

        //Give the pages back to the kernel
        munmap(pageHeader, totalSize);  
    }else{ //Put it in the free list
        ptrHeader -> freed = 1;
        ptrHeader -> next = pageHeader -> freeList;
        pageHeader -> freeList = ptrHeader;
    }
    pthread_mutex_unlock(&lock); //Unlock
}