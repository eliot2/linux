/*  Name: Eliot Carney-Seim
    Project: Project 1
    Description: Testing edge cases and mmap.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/user.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/user.h> 

#define CHECK_IS_NOT_NULL(ptrA) \
  do { \
  if ((ptrA) != NULL) { \
  test_passed++; \
  } else { \
  test_failed++; \
  printf("%d: FAIL\n", __LINE__); \
  } \
  } while(0);

#define CHECK_IS_EQUAL(valA,valB) \
  do { \
  if ((valA) == (valB)) { \
  test_passed++; \
  } else { \
  test_failed++; \
  printf("%d: FAIL\n", __LINE__); \
  } \
  } while(0);

#define CHECK_IS_NOT_EQUAL(valA,valB) \
  do { \
  if ((valA) != (valB)) { \
  test_passed++; \
  } else { \
  test_failed++; \
  printf("%d: FAIL\n", __LINE__); \
  } \
  } while(0);



int main(void) {
        unsigned test_passed = 0;
        unsigned test_failed = 0;
	
	char *babbleFile = "/dev/babbler";
	char *topicFile = "/dev/babbler_ctl";
 
	printf("Test 1: Test opening babbler!\n");	
	//Open file
	int fd1 = open(babbleFile, O_RDWR, S_IRUSR | S_IWUSR);
	CHECK_IS_NOT_EQUAL(fd1, -1);

	printf("Test 2: Test opening READ-ONLYbabbler-ctl\n");
	int fd2 = open(topicFile, O_RDONLY, S_IRUSR);
	CHECK_IS_NOT_EQUAL(fd2, -1);

	printf("Test 3: Test MMAP ret on Babbler\n");
	//Execute mmap
	void *mmappedData1 = mmap(NULL, 140, PROT_READ | PROT_WRITE, 
				  MAP_SHARED, fd1, 0);
	CHECK_IS_NOT_EQUAL(mmappedData1, MAP_FAILED);

	printf("Test 4: Test MMAP ret on Babbler-ctl\n");
	//Execute mmap
	void *mmappedData2 = mmap(NULL, 8, PROT_READ, 
				 MAP_PRIVATE | 0x8000, fd2, 0);
	CHECK_IS_NOT_EQUAL(mmappedData2, MAP_FAILED);

	printf("Test 5: Test Read from Babble-ctl\n");
	char* tmpStr = (char *)mmappedData2;
        CHECK_IS_EQUAL("#cs421", tmpStr); 
        //Above assumed "echo -n '#cs421'" command was given. 

	printf("Test 6: Test Write to Babbler\n");
	char *cpyRet = strcpy(mmappedData1, "I <3 #cs421");
	CHECK_IS_EQUAL(cpyRet, mmappedData1);
	
        printf("Test 7: Test too much write to babbler\n");
	char * BIG_STRING = "SHGVDHASGVDHGSVDSHGVSDHASDHGVSHGDVSHJDVASDDHGSAVDHSAVDHGSAVDHGSVDHASVDHSVHGDSVHGDASVSHDVHGSAVDHSAVFHDV FHGSDVDHSVDHGSVDHGASVDGJASVDJSVJDVASDFSAVFDSVFHDGVFJDSVFDGHVFDHGVFJVFDSGHVFDJV #cs421";
	cpyRet = strcpy(mmappedData1, BIG_STRING);
	CHECK_IS_EQUAL(cpyRet, mmappedData1);
	
	printf("Test 8: Test EDGECASE: Read from Babble, post BIG STRING\n");
	tmpStr = (char *)mmappedData1;
        CHECK_IS_EQUAL(BIG_STRING, tmpStr); 

	//Cleanup
	int rc1 = munmap(mmappedData1, 140);rc1++;rc1--;
       	int rc2 = munmap(mmappedData2, 8);rc2++;rc2--;
	
	close(fd1);
	close(fd2);

	printf("Total passes: %d\n", (unsigned int)test_passed);  	
	printf("Total fails: %d\n",  (unsigned int)test_failed);  
	
	return 0;
} 
