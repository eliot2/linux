/*  Name: Eliot Carney-Seim
    Project: Project 1
    Description: Testing edge cases and mmap.
*/

#include <pthread.h>
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

int fd1;
int fd2;
unsigned test_passed = 0;
unsigned test_failed = 0;
int err;

struct writeData {
	char * toWrite;
	int count;
} babbleData;
 
/*This function is intenionally racist, to test RC on Babbler/(-ctl)*/
static void *makeBabblerWrites(void *arguments)
{
	struct writeData *args = (struct writeData *)arguments;
	err = write(fd1, args->toWrite, 140);
	if(err < 0 || err != 140){
		printf("Error RC-Babbler Writing. Err: %d\n", err);
		CHECK_IS_EQUAL(0, 1);
	}
	args->toWrite[7] = 'a' + args->count;
	args->count++;

	return NULL;
}

/*This function is intenionally racist, to test RC on Babbler/(-ctl)*/
static void *makeCtlWrites(void *arguments)
{
	struct writeData *args = (struct writeData *)arguments;
	err = write(fd2, args->toWrite, 8);
	if(err < 0 || err != 8){
		printf("Error RC-Babbler-Ctl Writing. Err: %d\n", err);
		CHECK_IS_EQUAL(0, 1);
	}
	args->toWrite[7] = 'a' + args->count;
	args->count++;

	return NULL;
}



int main(void) {
	
	char *babbleFile = "/dev/babbler";
	char *topicFile = "/dev/babbler_ctl";
 
	printf("Test 1: Test opening babbler!\n");	
	//Open file
	fd1 = open(babbleFile, O_RDWR, S_IRUSR | S_IWUSR);
	CHECK_IS_NOT_EQUAL(fd1, -1);

	printf("Test 2: Test opening READ-ONLY babbler-ctl (SHOULD FAIL)\n");
	fd2 = open(topicFile, O_RDWR, S_IRUSR | S_IWUSR);
	CHECK_IS_NOT_EQUAL(fd2, -1);

	printf("Test 3: Test MMAP ret on Babbler (SHOULD FAIL)\n");
	//Execute mmap
	void *mmappedData1 = mmap(NULL, 140, PROT_READ | PROT_WRITE, 
				  MAP_SHARED, fd1, 0);
	CHECK_IS_EQUAL(mmappedData1, MAP_FAILED);

	printf("Test 4: Test MMAP ret on Babbler-ctl\n");
	//Execute mmap
	void *mmappedData2 = mmap(NULL, 8, PROT_READ, 
				 MAP_PRIVATE | 0x8000, fd2, 0);
	CHECK_IS_NOT_EQUAL(mmappedData2, MAP_FAILED);

	printf("Test 5: Test Read from Babble-ctl\n");
	char* tmpStr = (char *)mmappedData2;
	char* topicStr = "#cs421";
	CHECK_IS_EQUAL(0, strcmp(tmpStr, topicStr));
	printf("Read from Topic is: %s\n", tmpStr);
        //Above assumed "echo -n '#cs421'" command was given. 

	printf("Test 6: Test Write to Babbler-Ctl\n");
	size_t writeRet = write(fd2, "#cs421", 6);	
	CHECK_IS_EQUAL((int)writeRet, 6);

	printf("Test 7: Test Write to Babbler\n");
	writeRet = write(fd1, "I <3 #cs421", 11);	
	CHECK_IS_EQUAL((int)writeRet, 11);

	printf("Test 8: Test Read from Babbler\n");
	char readBabbler[140];
	size_t readRet = read(fd1, readBabbler, 140);	
	CHECK_IS_EQUAL((int)readRet, 11);	
	printf("readBabbler is: %s\n", readBabbler);


	printf("Test 9: Test too much write to babbler\n");
	char * BIG_STRING = "SHGVDHASGVDHGSVDSHGVSDHASDHGVSHGDVSHJDVASDDHGSAVDHSAVDHGSAVDHGSVDHASVDHSVHGDSVHGDASVSHDVHGSAVDHSAVFHDV FHGSDVDHSVDHGSVDHGASVDGJASVDJSVJDVASDFSAVFDSVFHDGVFJDSVFDGHVFDHGVFJVFDSGHVFDJV #cs421";
	writeRet = write(fd1, BIG_STRING, 188);
	CHECK_IS_NOT_EQUAL(writeRet, 188);
	
	printf("Test 10: Test EDGECASE: Read from Babble, post BIG STRING\n");
	char data[189];
	err = read(fd1, data, 188);
	if(err < 0){
		printf("Error in read.\n");
		CHECK_IS_EQUAL(0, 1);
	}	
	data[188] = '\0';
        CHECK_IS_NOT_EQUAL(0, strcmp(BIG_STRING, data)); 

	printf("Test 11: Test Race Condition: Writing to Babbler\n");
	/*generating test variables*/
	int totalThreads = 25;
	pthread_t writeThreads [totalThreads];
	char * a_write = malloc(10*sizeof(char));
	strcpy(a_write, "#cs421 a\n\0");
	babbleData.toWrite = a_write;
	/*spamming driver with writes*/
	for(size_t i = 0; i < totalThreads; i++){
		pthread_create(writeThreads + i, NULL, 
			       makeBabblerWrites, &babbleData);
	}
	for(size_t i = 0; i < totalThreads; i++){
		pthread_join(writeThreads[i], NULL);
	}
	/*testing read results of thread burst attack*/
	char *babblerContent = malloc(141*sizeof(char));
	err = read(fd1, babblerContent, 140);
	babblerContent[140] = '\0'; 
	if(err < 0 || err != 140){
		printf("Error reading Babbler. Err: %d\n", err);
		CHECK_IS_EQUAL(0, 1);
	}else{
		printf("Read Content from Babbler is: %s\n", babblerContent);
		CHECK_IS_EQUAL(1, 1);
	}
	free(babblerContent);

	printf("Test 12: Test Race Condition: Writing to Babbler-Ctl\n");
	/*resetting test variables*/
	free(a_write);
	a_write = malloc(8*sizeof(char));
	strncpy(a_write, "#cs421 a", 8);
	babbleData.toWrite = a_write;
	babbleData.count = 0;
	/*spamming driver with writes*/
	for(size_t i = 0; i < totalThreads; i++){
		pthread_create(writeThreads + i, NULL, 
			       makeCtlWrites, &babbleData);
	}
	for(size_t i = 0; i < totalThreads; i++){
		pthread_join(writeThreads[i], NULL);
	}
	free(a_write);	
	/*testing read results of thread burst attack*/
	char *ctlContent = malloc(8*sizeof(char));
        strncpy(ctlContent, (char *)mmappedData2, 8);       
	if(strncmp(ctlContent, "#cs421 x", 8) != 0){
		printf("Error reading Babbler-ctl. Err: %d\n", err);
		CHECK_IS_EQUAL(0, 1);
	}else{
		printf("Read Content from Babbler-ctl is: %s\n", ctlContent);
		CHECK_IS_EQUAL(1, 1);
	}

	//Cleanup
	int rc1 = munmap(mmappedData1, 140);rc1++;rc1--;
       	int rc2 = munmap(mmappedData2, 8);rc2++;rc2--;
	
	close(fd1);
	close(fd2);

	printf("Total passes: %d\n", (unsigned int)test_passed);  	
	printf("Total fails: %d\n",  (unsigned int)test_failed);

	return 0;
} 
