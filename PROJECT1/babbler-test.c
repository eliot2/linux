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

int main(void) {

	char *babbleFile = "/dev/babbler";
	char *topicFile = "/dev/babbler-ctl";
 
	printf("Test 1: Opening devices!\n");
	
	//Open file
	int fd = open(babbleFile, O_RDRW, 0);
	if(fd == -1)
		return -1;


	//Execute mmap
	void* mmappedData = mmap(NULL, 1 * PAGESIZE, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
	
	if(mmappedData == MAP_FAILED)
		return -1;

	//Write the mmapped data to stdout (= FD #1)
	write(1, mmappedData, filesize);
	//Cleanup
	int rc = munmap(mmappedData, filesize);
	assert(rc == 0);
	close(fd);
	
	return 0;
} 
