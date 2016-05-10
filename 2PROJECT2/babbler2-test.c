/*  Name: Eliot Carney-Seim
    Project: Project 2
    Description: Testing edge cases and mmap.
*/

#define _POSIX_SOURCE

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
	int fd1 = open(babbleFile, O_RDWR, S_IRUSR | S_IWUSR);
	CHECK_IS_NOT_EQUAL(fd1, -1);

	printf("Test 2: Test opening READ-ONLYbabbler-ctl\n");
	int fd2 = open(topicFile, O_RDONLY, S_IRUSR);
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
        CHECK_IS_EQUAL("#cs421", tmpStr); 
        //Above assumed "echo -n '#cs421'" command was given. 

	printf("Test 6: Test Write to Babbler\n");
	char *cpyRet = write(fd1, "I <3 #cs421", 11);
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
	

	if (babblenet_socket >= 0) {
		close(babblenet_socket);
	}
	return 0;
}
