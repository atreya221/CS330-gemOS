#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#define ull unsigned long long

int main(int argc, char* argv[])
{
	ull res;

	if(!strcmp(argv[0], "./root"))
	{
		res = atoi(argv[argc-1]);
		char tmp[256] = {'.', '/'};
		argv[1] = strcat(tmp, argv[1]);
	}
	else
	{
		printf("UNABLE TO EXECUTE\n");
		return 0;
	}


    double acc = sqrt((double)res);
	res = round(acc);

	char *tempstr;
    tempstr[0] = '\0';

    sprintf(tempstr, "%lld", res);
	strcpy(argv[argc-1], tempstr);

	if(argc > 2)
	{
		if( strcmp(argv[1], "./root") && strcmp(argv[1], "./square") && strcmp(argv[1], "./double"))
		{
			printf("UNABLE TO EXECUTE\n");
		}
		else
		{
			if(execv(argv[1], argv+1))
			{
				perror("exec");
				exit(-1);
			}
		}
	}
	else
	{
		printf("%lld\n", res);
	}
	return 0;
}