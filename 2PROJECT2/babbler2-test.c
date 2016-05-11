/*  Name: Eliot Carney-Seim
    Project: Project 2
    Description: Testing edge cases and mmap.
*/

#define _POSIX_SOURCE

#include <pthread.h>
#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/user.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/user.h> 

#define BABBLENET_PORT 4210
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
 
/*This function is intenionally racist, to test RC on Babbler/(-ctl)*/
static void *makeBabblerWrites(void *toWrite)
{
	err = write(fd1, toWrite, 140);
	if(err < 0 || err != 140){
		printf("Error RC-Babbler Writing. Err: %d\n", err);
		CHECK_IS_EQUAL(0, 1);
	}else{
		printf("I am a thread writing: %s\n", (char *)toWrite);		
	}
	return NULL;
}


static int babblenet_socket = -1;



/**
 * Initializes the network connection to the BabbleNet server.
 *
 * If unable to connect to the server, then display an error message
 * and abort the program.
 *
 * You do not need to modify this function.
 */
static void init_babblenet_connection(void)
{
	struct addrinfo hints, *result, *p;


	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	int ret = getaddrinfo("localhost", "4210", &hints, &result);
	if (ret) {
		fprintf(stderr, "Could not resolve localhost: %s\n", gai_strerror(ret));
		exit(EXIT_FAILURE);
	}

	for (p = result; p; p = p->ai_next) {
		babblenet_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (babblenet_socket < 0) {
			continue;
		}
		if (connect(babblenet_socket, p->ai_addr, p->ai_addrlen) >= 0) {
			break;
		}
		close(babblenet_socket);
		babblenet_socket = -1;
	}

	freeaddrinfo(result);
	if (babblenet_socket < 0) {
		fprintf(stderr, "Could not connect to BabbleNet\n");
		exit(EXIT_FAILURE);
	}
}

/*This function is intenionally racist, to test RC on Babbler/(-ctl)*/
static void *makeCtlWrites(void *toWrite)
{
	int err = write(fd2, toWrite, 8);
	if(err < 0 || err != 140){
		printf("Error RC-Ctl Writing.\n");
		CHECK_IS_EQUAL(0, 1);
	}
	return NULL;
}

/**
 * Send a message to the BabbleNet server.
 *
 * You do not need to modify this function.
 *
 * @param babble buffer containing babble to send
 * @param buffer_len number of bytes from @babble to send
 *
 * @return true if babble was successfully sent, false if not
 */
static bool babblenet_send(const void *babble, size_t buffer_len)
{
	if (babblenet_socket < 0) {
		fprintf(stderr, "Not connected to BabbleNet\n");
		return false;
	}

	ssize_t retval;
	do {
		retval = write(babblenet_socket, babble, buffer_len);
	} while (retval < 0 && errno == EINTR);

	if (retval < 0) {
		perror("failed to send babble");
		return false;
	}
	else if (retval != buffer_len) {
		fprintf(stderr, "unable to send %zu bytes (only sent %zu)\n", buffer_len, retval);
		return false;
	}
	return true;
}


       
int main(void) {
        unsigned test_passed = 0;
	unsigned test_failed = 0;
	


	init_babblenet_connection();

	char *babbleFile = "/dev/babbler";
	char *topicFile = "/dev/babbler_ctl";
 
	printf("Test 1: Test opening babbler!\n");	
	//Open file
	fd1 = open(babbleFile, O_RDWR, S_IRUSR | S_IWUSR);
	CHECK_IS_NOT_EQUAL(fd1, -1);

	printf("Test 2: Test opening READ-ONLYbabbler-ctl\n");
	fd2 = open(topicFile, O_RDONLY, S_IRUSR);
	CHECK_IS_NOT_EQUAL(fd2, -1);

	printf("Test 3: Test MMAP ret on Babbler\n");
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

	printf("Test 6: Test Write to Babbler\n");
	size_t writeRet = write(fd1, "I <3 #cs421", 11);	
	CHECK_IS_EQUAL((int)writeRet, 11);

	printf("Test 7: Test Read from Babbler\n");
	char readBabbler[140];
	size_t readRet = read(fd1, readBabbler, 140);	
	CHECK_IS_EQUAL((int)readRet, 11);	
	printf("readBabbler is: %s\n", readBabbler);


	printf("Test 8: Test too much write to babbler\n");
	char * BIG_STRING = "SHGVDHASGVDHGSVDSHGVSDHASDHGVSHGDVSHJDVASDDHGSAVDHSAVDHGSAVDHGSVDHASVDHSVHGDSVHGDASVSHDVHGSAVDHSAVFHDV FHGSDVDHSVDHGSVDHGASVDGJASVDJSVJDVASDFSAVFDSVFHDGVFJDSVFDGHVFDHGVFJVFDSGHVFDJV #cs421";
	writeRet = write(fd1, BIG_STRING, 188);
	CHECK_IS_NOT_EQUAL(writeRet, 188);
	
	printf("Test 9: Test EDGECASE: Read from Babble, post BIG STRING\n");
	char data[189];
	err = read(fd1, data, 188);
	if(err < 0){
		printf("Error in read.\n");
		CHECK_IS_EQUAL(0, 1);
	}	
	data[188] = '\0';
        CHECK_IS_NOT_EQUAL(0, strcmp(BIG_STRING, data)); 

	printf("Test 10: Test Race Condition: Writing to Babbler\n");
	/*generating test variables*/
	char * a_write = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa#cs421";
	char * b_write = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb#cs421";
	char * c_write = "ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc#cs421";
	pthread_t writeThreads [3];
	char * writeContent [3] = {a_write, b_write, c_write};
	for(size_t i = 0; i < 1; i++){
		pthread_create(writeThreads + i, NULL, 
			       makeBabblerWrites, writeContent[i]);
	}
	for(size_t i = 0; i < 1; i++){
		pthread_join(writeThreads[i], NULL);
	}

	char babblerContent[140];
	err = read(fd1, babblerContent, 140);
	babblerContent[139] = '\0'; 
	if(err < 0 || err != 140){
		printf("Error reading Babbler. Err: %d\n", err);
		CHECK_IS_EQUAL(0, 1);
	}else{
		printf("Read Content from Babbler is: %s\n", babblerContent);
	}
	
	int totalFails = 0;
	if(strcmp(a_write, babblerContent) != 0)
		totalFails++;
	if(strcmp(b_write, babblerContent) != 0)
		totalFails++;
	if(strcmp(b_write, babblerContent) != 0)
		totalFails++;
	if(totalFails == 3)
		CHECK_IS_EQUAL(0, 1); 

	//Cleanup
	int rc1 = munmap(mmappedData1, 140);rc1++;rc1--;
       	int rc2 = munmap(mmappedData2, 8);rc2++;rc2--;
	
	close(fd1);
	close(fd2);

	printf("Total passes: %d\n", (unsigned int)test_passed);  	
	printf("Total fails: %d\n",  (unsigned int)test_failed);  
	

	if (babblenet_socket >= 0) {
		close(babblenet_socket);
	}
	return 0;
}
