
#include <stdio.h>
#include <string.h>

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
	func3(func1(), func2());
	
	return (int)0;
}
