
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char * func1()
{
	return "Hello";
}

char * func2()
{
	return "World";
}

void func3(char * string1, char * string2)
{
	printf("%s %s\n", string1, string2);
}

int main()
{

	char * T = (char *)malloc (4);
	char * Y = T+2;
	
	*Y = 'e';

	printf("%c is my char\n", T[2]);

	func3(func1(), func2());
	

	free(T);
	return (int)0;
}
