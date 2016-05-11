#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main(void){

	
	char a = 'a';
	char * topic = malloc(10*sizeof(char));
	strcpy(topic, "#cs421 a\n\0");
	topic[7] = a + 1;
	printf("Topic is: %s", topic);

	if("cars" != "cers"){
		printf("pOOP");
	}
  
	for(int i = 0; i < 5; i++){
		printf("%d\n", i);
	}
	
	free(topic);
	return 0;
}
