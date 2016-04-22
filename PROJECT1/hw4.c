/* Name: Eliot Carney-Seim
 * Project: HW4
 * Description: This program emulates First-Fit memory allocation, written from
 * scratch.
 * Sources:
 * http://stackoverflow.com/questions/18986351/what-is-the-simplest-standard-
 * conform-way-to-produce-a-segfault-in-c
 * http://stackoverflow.com/questions/26805461/why-do-i-get-cast-from-pointer
 * -to-integer-of-different-size-error/26805723#26805723
 *
 * NOTE: EXTRA CREDIT ATTEMPTED!!
 */
#define _POSIX_C_SOURCE 200809L



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>

#if __linux__   //  or #if __GNUC__
    #if __x86_64__ || __ppc64__
        #define ENVIRONMENT64
    #else
        #define ENVIRONMENT32
    #endif
#else
    #if _WIN32
        #define ENVIRONMENT32
    #else
        #define ENVIRONMENT64
    #endif
#endif // __linux__

#ifdef ENVIRONMENT64
    #define MAX_BLOCK_SIZE unsigned long long int
#else
    #define MAX_BLOCK_SIZE unsigned long int
#endif // ENVIRONMENT64

/*HW4 QUESTIONS:
 *
 * How would a call to my_free() would know to deallocate 5 frames
 * if the user had previously called my_malloc() for 150 bytes?
 *  - PAGE_FRAME_ALLOC tracks each frame to see if it's been allocated
 *    but also tracks the starting block allocated and how much.
 * How does my_free() know it needs to send a SEGFAULT if the user tries
 * to free a pointer that points into the middle of a memory block?
 *  - From the starting address, each memory block begins at a segment of 32
 *    bytes for 16 blocks. If the given pointer is not a multiple of 32 from
 *    the starting address, then it's not pointing to the start of a page frame.
 *
*/

const int TOTAL_FRAMES = 16;
const int FRAME_SIZE = 32;
unsigned char MEMORY[16*32]; //Indexed in multiples of 32 only.
int  METADATA[16] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
              //-1 Means empty. 0 means allocated. >0 means pages allocated.

/**
 * Change the size of the memory block pointed to by @a ptr.
 *
 * - If @a ptr is @c NULL, then treat this as if a call to
 *   my_malloc() for the requested size.
 * - Else if @a size is @c 0, then treat this as if a call to
 *   my_free().
 * - Else if @a ptr is not a pointer returned by my_malloc() or
 *   my_realloc(), then send a SIGSEGV signal to the calling process.
 *
 * Otherwise reallocate @a ptr as follows:
 *
 * - If @a size is smaller than the previously allocated size, then
 *   reduce the size of the memory block. Mark the excess memory as
 *   available. Memory sizes are rounded up to the next 32-byte
 *   increment.
 * - If @a size is the same size as the previously allocated size,
 *   then do nothing.
 * - If @a size is greater than the previously allocated size, then
 *   allocate a new contiguous block of at least @a size bytes,
 *   rounded up to the next 32-byte increment. Copy the contents from
 *   the old to the new block, then free the old block.
 *
 * @param ptr Pointer to memory region to reallocate.
 * @param size Number of bytes to reallocate.
 *
 * @return If allocating a new memory block or if resizing a block,
 * then pointer to allocated memory; @a ptr will become invalid. If
 * freeing a memory region or if allocation fails, return @c NULL. If
 * out of memory, set errno to @c ENOMEM.
 */
void *my_realloc(void *ptr, size_t size);

/**
 * Allocate and return a contiguous memory block that is within the
 * memory region.
 *
 * The size of the returned block will be at least @a size bytes,
 * rounded up to the next 32-byte increment.
 *
 * @param size Number of bytes to allocate. If @c 0, your code may do
 * whatever it wants; my_malloc() of @c 0 is "implementation defined",
 * meaning it is up to you if you want to return @c NULL, segfault,
 * whatever.
 *
 * @return Pointer to allocated memory, or @c NULL if no space could
 * be found. If out of memory, set errno to @c ENOMEM.
 */
void *my_malloc(size_t size);

/**
 * Deallocate a memory region that was returned by my_malloc() or
 * my_realloc().
 *
 * If @a ptr is not a pointer returned by my_malloc() or my_realloc(),
 * then send a SIGSEGV signal to the calling process. Likewise,
 * calling my_free() on a previously freed region results in a
 * SIGSEGV.
 *
 * @param ptr Pointer to memory region to free. If @c NULL, do
 * nothing.
 */
void my_free(void *ptr);

/**
 * Display information about memory allocations to standard output.
 *
 * Display to standard output the following:
 * - Memory contents, one frame per line, 16 lines total. Display the
 *   actual bytes stored in memory. If the byte is unprintable (ASCII
 *   value less than 32 or greater than 126), then display a dot
 *   instead.
 * - Current memory allocations, one line of 32 characters, where each
 *   character corresponds to a frame. Indicate reserved frames with
 *   R, free memory with f.
 */
void my_malloc_stats(void);

/**
 * @brief initializeMemory zeros out our MEMORY
 */
void initializeMemory();

/**
 * @brief my_empty_pages prints out PAGE_FRAME_ALLOC in neat readable style.
 */
void my_empty_pages();

/**
 * @brief testPointer tests to see if a given pointer is a MEMORY page frame.
 * @param ptr - expecting ptr to page frame start.
 * @return boolean - 1 if ptr is indeed pointing to MEMORY page frame 0 if not.
 */
static int isValidPtr( void *ptr);

/**
 * @brief getMemIndex gets index of ptr's page frame in MEMORY, exits if bad ptr
 * @param ptr a pointer pointing to a page frame in MEMORY
 * @return int index of page from 0 - 15 in MEMORY, -1 if bad, ALTHOUGH this
 * function assumes it will get a valid pointer.
 */
static int getMemIndex(void *ptr);

/**
 * Unit test of your memory allocator implementation. This will
 * allocate and free memory regions.
 */
extern void hw4_test(void);

/**
 * Format a string into a newly allocated memory block.
 *
 * Calculate the length that a formatted string (as per snprintf())
 * would be, then allocate contiguous memory block(s) of at least that
 * length. Then output the resulting string, including terminating
 * null character ('\0') to that memory. The starting address of the
 * memory block is written to the pointer pointed to by @a strp. The
 * pointer can be freed via my_free() afterwards.
 *
 * @param strp Pointer to where to write pointer to.
 *
 * @param fmt Format specification, as per snprintf().
 *
 * @return Number of bytes allocated (excluding terminating null
 * byte), if memory allocation succeeds. If out of memory, @a strp is
 * @c NULL, or some other error occurs, return -1.
 */
int my_asprintf(char **strp, const char *fmt, ...);

int main(void)
{
//    initializeMemory();
//    hw4_test();

    char * mc = "HEYY";
    char **mmc = &mc;
    my_asprintf(mmc, "HEYYYY");

    printf("%s", *mmc);
    my_empty_pages();
    my_malloc_stats();
//    char * myChars = my_malloc(32*5);
//    strcpy(myChars, "ABCDE");
//    my_empty_pages();
//    my_malloc_stats();
//    my_free(myChars);
//    my_empty_pages();
//    my_malloc_stats();
//    my_free(myChars);
//    myChars = my_realloc(myChars, 32*3);
//    my_empty_pages();
//    my_malloc_stats();
//    char * myChars2 = my_malloc(32*5);
//    strcpy(myChars2, "XXXXXX");
//    my_empty_pages();
//    my_malloc_stats();
    return 0;
}

int my_asprintf(char **strp, const char *fmt, ...){
    int ptrSize = sizeof(fmt);

    int myBool = 1;
    int fmtLength = 0;
    int i = 0;
    while(myBool){
        if(((int)fmt[i]) == (int)'\0'){
            break;
        }else{
            i++;
            fmtLength++;
        }
    }

    *strp  = my_malloc(fmtLength+1);

    int newIndex = getMemIndex(*strp);
    int newMemAdr = newIndex*FRAME_SIZE;
    for(i = 0; i < fmtLength; i++){
        MEMORY[newMemAdr+i] = fmt[i];
    }
    return fmtLength;
}

static int getMemIndex(void * ptr){
    if(!isValidPtr(ptr)){
        //NOT Valid pointer!
        return -1;//LOOKING FOR DIVISION BY 32 WITHOUT ADDING FOR REMAINDER
    }
    return (MAX_BLOCK_SIZE)(((unsigned char *)ptr) - &MEMORY[0])/FRAME_SIZE;
}

int isValidPtr(void *ptr){
    unsigned int ptrAddress = (MAX_BLOCK_SIZE)ptr;
    unsigned int startAddressBlock = (MAX_BLOCK_SIZE)&MEMORY[0];
    unsigned int endAddressBlock = (MAX_BLOCK_SIZE)&MEMORY[511];
    if(ptrAddress <  startAddressBlock|| ptrAddress > endAddressBlock){
        printf("Attempted to free address outside of MEMORY.\n");
        printf("0 Address location: %d\n", startAddressBlock);
        printf("511 Address location: %d\n", endAddressBlock);
        printf("Attempted Address location: %d\n", ptrAddress);
        raise(SIGSEGV);
        return 0;
    }

    //If address passes inspection, free those pages.
    //Find difference in address location to get the frame location.
    unsigned int addressInMEMORY = ptrAddress - startAddressBlock;
    if(addressInMEMORY%32 != 0){
     printf("Invalid address given. Does not point to page frame in MEMORY.\n");
        raise(SIGSEGV);
        return 0;
    }
    return 1;
}

void *my_realloc(void *ptr, size_t size){
    int intSize = (int)size;
    if(ptr == NULL){
        //zero size not tested because handled in malloc.
        return my_malloc(size);
    }else if (intSize == 0){
        my_free(ptr);
        //proper pointer handled by free, no need to test here.
        return NULL;
    }else if(!isValidPtr(ptr)){
        //pointer test not passed!
        return NULL;
    }

    int pageIndex = getMemIndex(ptr);

    unsigned char tempMem[intSize]; // size = num bytes
    // METADATA[pageIndex] tells us how many pages a ptr has, then * FRAME_SIZE
    // gets us total bytes.
    int currentPagesAllocated = METADATA[pageIndex];
    //pages = bytes/32 + 1 IF remainder exists
    int newPagesAllocated = intSize/FRAME_SIZE + ((intSize%FRAME_SIZE) ? 1 : 0);

    //if new size is less than currently allocated size
    if(newPagesAllocated < currentPagesAllocated){
        int differenceAllocated = currentPagesAllocated - newPagesAllocated;

        //mark unused blocks as free to write on
        int i;
        int block;
        for(i = differenceAllocated; i > 0; i--){
            block = pageIndex+newPagesAllocated+i - 1;
            //we subtract 1 so as to not include the header block.
//            printf("Freeing block: %d", block);
            METADATA[block]= -1;
        }
//        printf("\n");
        METADATA[pageIndex] = newPagesAllocated;
        return ptr;
    }else if (newPagesAllocated == currentPagesAllocated){
        return ptr;
    }else{
        //NEW ALLOCATION > CURRENT ALLOCATION
        //back up old data
        void* newPtr = my_malloc(size);
        int newIndex = getMemIndex(newPtr);
        int newMemAdr = newIndex*FRAME_SIZE; // new location in MEMORY
        int oldMemAdr = pageIndex*FRAME_SIZE; // old location in MEMORY
        int i;
        //copy down all bytes currently allocated.
        for(i = 0; i < currentPagesAllocated*FRAME_SIZE; i++){
            //copy each byte into new memory location
            MEMORY[newMemAdr + i] = MEMORY[oldMemAdr + i];
        }
        my_free(ptr); //marking excess memory as available is also here.
        //store reallocated data into new page frame.
        return newPtr;
    }

    //NOTE TO GRADER: errno exit on ENOMEM is handled inside the my_malloc
    // called in this script.
}

void my_free(void *ptr){

    if(!isValidPtr(ptr)){
        return;
        //pointer test not passed!
    }
    if(METADATA[getMemIndex(ptr)] == -1){
        printf("Error: Free on already free block.\n");
        raise(SIGSEGV);
        return;
    }
    //get the amount of pages allocated.
    int pageIndex = getMemIndex(ptr);
    unsigned int pagesAllocated = METADATA[pageIndex];

    //free them
//    printf("Starting index: %d \n", pageIndex);
//    printf("Removing:");
    for(; pagesAllocated > 0; pagesAllocated--){
//        printf("%d,", pageIndex+pagesAllocated-1);
        METADATA[pageIndex+pagesAllocated-1] = -1;
    }
//    printf("\n");
}

void initializeMemory(){
    int i;
    int j;
    for(i = 0; i < TOTAL_FRAMES; i++){
        for(j = 0; j < FRAME_SIZE; j++){
            MEMORY[(i*FRAME_SIZE)+j] = 0;
        }
    }
}

void my_empty_pages(){
    int i;
    for(i = 0; i < TOTAL_FRAMES; i++){
        printf("|%2d|", METADATA[i]);
    }printf("\n");
}

void my_malloc_stats(){
    int i;
    int asciiValue;
    for(i = 0; i < 512; i++){
        asciiValue = (int)MEMORY[i];
        if(asciiValue < 32 || asciiValue > 126){
            printf("."); //print "." for entire empty frame.
        }else{
            char byteData = MEMORY[i];
            printf("%c", byteData);
        }
        //NEWLINE EVERY 32 CHARS
        if((i+1)%FRAME_SIZE==0 && i != 0){
            printf("\n");
        }
    }printf("\n");
}

void *my_malloc(size_t size){
    int i;

    int intSize = (int)size;
    //pages = bytes/32 + 1 IF remainder exists
    int pagesToAlloc = intSize/FRAME_SIZE + ((intSize%FRAME_SIZE) ? 1 : 0);
    if(pagesToAlloc > TOTAL_FRAMES || pagesToAlloc == 0){
        printf("MEMORY only has 16 32byte pages, you tried to allocate %d\n",
               pagesToAlloc);
        errno = ENOMEM;
        return NULL;
    }


    printf("Pages to allocate: %d\n", pagesToAlloc);
    //search for enough page frames to fit.
    int totalPagesFound = 0;
    void *pageAddress;
    for(i = 0; i < TOTAL_FRAMES; i++){
        if(METADATA[i] == -1){
            totalPagesFound++;

            if(totalPagesFound == pagesToAlloc){
                //go back and mark all pages found as non-empty
                //and get address of first page.
                for(; totalPagesFound > 0; i--){
                    METADATA[i] = 0;
                    totalPagesFound --;
                }
                i++; //correct for an additional i--
                pageAddress = &MEMORY[i*FRAME_SIZE];
                METADATA[i] = pagesToAlloc;
                return pageAddress;
            }
        }else{
            totalPagesFound = 0;
        }
    }
    printf("WARNING: NO SPACE IN MEMORY.\n");
    errno = ENOMEM;
    return NULL;
}


