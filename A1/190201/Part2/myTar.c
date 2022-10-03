#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

void tar(char* path, char* resfile)  // creates a tar of contents of specified directory within the directory
{
	DIR* dirptr = opendir(path);
	if(!dirptr)
	{
		printf("Failed to complete creation operation\n");
		return;
	}

	char tarfile[256] = {'\0'};
	strcat(tarfile, path);
	strcat(tarfile, "/");
	strcat(tarfile, resfile);
	//printf("%s\n", tarfile);

	int fdtar;
	if((fdtar = open(tarfile, O_RDWR | O_CREAT, 0644)) < 0)
	{
		printf("Failed to complete creation operation\n");
		return;
	}

	struct dirent *node;
	while ((node = readdir (dirptr)) != NULL) 
	{
		if(node->d_type != 4)  // not '.' or '..'
		{
			char filename[17] = {'\0'};
			strcat(filename, node->d_name);

			char filepath[256] = {'\0'};
			strcat(filepath, path);
			strcat(filepath, "/");
			strcat(filepath, filename);

			int fp = open(filepath, O_RDONLY);
			if (fp < 0) 
			{
				printf("Failed to complete creation operation\n");
				return;
			}
			if(!strcmp(filepath, tarfile)) continue;
			int sz = lseek(fp, 0, SEEK_END);
			char size[11];
			size[0]='\0';
			sprintf(size, "%d", sz);
			lseek(fp, 0, SEEK_SET);

			int filenamesz = strlen(filename);
			char fnsize[11] = {'\0'};

			sprintf(fnsize, "%d", filenamesz);
			if(write(fdtar, fnsize, strlen(fnsize)) < 0) 
			{
				printf("Failed to complete creation operation\n");
				return;
			}
			for(int i = strlen(fnsize); i < 2; i++)
			{
				if(write(fdtar, "$", 1) < 0) 
				{
					printf("Failed to complete creation operation\n");
					return;
				}
			}
			if(write(fdtar, filename, filenamesz) < 0)
			{
				printf("Failed to complete creation operation\n");
				return;
			}
			for(int i = filenamesz; i <= 15; i++)
			{
				if(write(fdtar, "$", 1) < 0) 
				{
					printf("Failed to complete creation operation\n");
					return;
				}
			}
			if(write(fdtar, size, strlen(size)) < 0)
			{
				printf("Failed to complete extraction operation\n");
				return;
			}
			for(int i = strlen(size); i <= 9; i++)
			{
				if(write(fdtar, "$", 1) < 0) 
				{
					printf("Failed to complete creation operation\n");
					return;
				}
			}
			char *buff = (char *) malloc((1e10+1) * sizeof(char));

			if(read(fp, buff, sz) < 0)
			{
				printf("Failed to complete creation operation\n");
				return;	
			}
			if(write(fdtar, buff, sz) < 0) 
			{
				printf("Failed to complete creation operation\n");
				return;
			}
		}
	}
}



int readint(char* str, int len) // decode number from encoded string (with '$')
{
	char *temp = (char *) malloc (20 * sizeof(char));
	for(int i = 0; i < len; i++)
	{
		if(temp[i] == '$')
		{
			temp[i] = '\0';
			break;
		}
		temp[i] = str[i];
	}

	int res = atoi(temp); // convert char-type string to int
	free(temp);
	return res;
}



void untar(char* path)  // untar from specified path
{	
	char destpath[256] = {'\0'};
	strcpy(destpath, path);

	int idx=0;
	for(int i = 0; i < strlen(path); i++)
	{
		if(path[i] == '.') 
		{
			idx = i;
		}
	}

	destpath[idx] = '\0';
	strcat(destpath, "Dump/");

	int chk = mkdir(destpath, 0777);
	DIR* dirPtr = opendir(destpath);
	if(!dirPtr)
	{
		printf("Failed to complete extraction operation\n");
		return;
	}

	int fdtar;
	if ((fdtar = open(path, O_RDONLY)) < 0) 
	{
		printf("Failed to complete extraction operation\n");
		return;
	}
	
	int tarsize = lseek(fdtar, 0, SEEK_END);
	lseek(fdtar, 0, SEEK_SET);
	int currsize = 0;
	
	while(currsize != tarsize)
	{
		char filenamesize[3];
		if(read(fdtar, filenamesize, 2) < 0) // read filename size
		{
			printf("Failed to complete extraction operation\n");
			return;
		}
		filenamesize[2] = '\0';
		//printf("%s\n", filenamesize);

		int fnsz = readint(filenamesize, 2);

		char fname[17];
		if(read(fdtar, fname, fnsz) < 0) // read filename
		{
			printf("Failed to complete extraction operation\n");
			return;
		}
		fname[fnsz] = '\0';
		lseek(fdtar, 16-fnsz, SEEK_CUR); // shift fileptr by appropriate offset

		char filesize[11];
		if(read(fdtar, filesize, 10) < 0)  // read file size
		{
			printf("Failed to complete extraction operation\n");
			return;
		}
		filesize[10] = '\0';
		int filesz = readint(filesize, 10);
		char *buff = (char *) malloc ((1e10+1) * sizeof(char));
		if(read(fdtar, buff, filesz) < 0) // read file content
		{
			printf("Failed to complete extraction operation\n");
			return;
		}
		char filepath[100] = {'\0'};
		strcat(filepath, destpath);
		strcat(filepath, fname);

		int fileptr = open(filepath, O_RDWR|O_CREAT, 0644);
		if(fileptr < 0)
		{
			printf("Failed to complete extraction operation\n");
			return;
		}

		if(write(fileptr, buff, filesz) < 0) // write content of buffer to created file
		{
			printf("Failed to complete extraction operation\n");
			return;
		}
		close(fileptr);
		
		currsize += (2+16+10+filesz); // 2 bytes for filename size, 16 bytes for file name, 10 bytes for file size and rest for file content
		free(buff);
	}
}



void untar_one(char *path, char *filename) // untar a specific file from tar file
{
	char destpath[100];
	strcpy(destpath, path);
	int idx=0;
	for(int i = 0; i < strlen(path); i++)
	{
		if(path[i] == '/') idx = i;
	}
	destpath[idx] = '\0';
	strcat(destpath, "/IndividualDump/");
	int chk = mkdir(destpath, 0777);
	DIR* dirPtr = opendir(destpath);
	if(!dirPtr)
	{
		printf("Failed to complete extraction operation\n");
		return;
	}
	int fdtar = open(path, O_RDONLY);
	if(fdtar < 0)
	{
		printf("Failed to complete extraction operation\n");
		return;
	}
	int tarsize = lseek(fdtar, 0, SEEK_END);

	lseek(fdtar, 0, SEEK_SET);
	int currsize = 0;
	//while(!feof(fdtar))
	int flag = 1;
	while(currsize != tarsize)
	{
		char filenamesize[3];
		if(read(fdtar, filenamesize, 2) < 0)
		{
			printf("Failed to complete extraction operation\n");
			return;
		}
		filenamesize[2] = '\0';

		int fnsz = readint(filenamesize, 2);
		char fname[17];
		if(read(fdtar, fname, fnsz) < 0)
		{
			printf("Failed to complete extraction operation\n");
			return;
		}
		fname[fnsz] = '\0';

		int ch = lseek(fdtar, 16-fnsz, SEEK_CUR);
		char filesize[11];

		if(read(fdtar, filesize, 10) < 0)
		{
			printf("Failed to complete extraction operation\n");
			return;
		}
		int filesz = readint(filesize, 10);
		char *buff = (char *)malloc((1e10+1) * sizeof(char));

		if (read(fdtar, buff, filesz) < 0)
		{
			printf("Failed to complete extraction operation\n");
			return;
		}
		char *filepath = (char *)malloc(256 * sizeof(char));
		strcat(filepath, destpath);
		strcat(filepath, fname);

		if(!strcmp(fname, filename))
		{
			flag = 1;
			int fileptr = open(filepath, O_RDWR | O_CREAT, 0644);
			if(fileptr < 0)
			{
				printf("Failed to complete extraction operation\n");
				return;
			}

			if(write(fileptr, buff, filesz) < 0)
			{
				printf("Failed to complete extraction operation\n");
				return;
			}
			close(fileptr);
			return;
		}
		currsize += (28+filesz);
		free(buff);
		free(filepath);
		//printf("%d\n", currsize);
	}
	printf("No such file is present in tar file.\n");
}



void enlist(char *path) // enlists the files encrypted in the tar file
{
	int fdtar = open(path, O_RDONLY);
	if(fdtar < 0)
	{
		printf("Failed to complete list operation\n");
		return;
	}
	int tarsize = lseek(fdtar, 0, SEEK_END);
	int check = lseek(fdtar, 0, SEEK_SET);
	int currsize = 0;
	int files = 0;
	char destpath[100];
	strcpy(destpath, path);
	int idx = 0;
	for(int i = 0; i < strlen(path); i++)
	{
		if(path[i] == '/') idx = i;
	}
	destpath[idx] = '\0';
	strcat(destpath, "/tarStructure");
	int fdwrite = open(destpath, O_RDWR | O_CREAT, 0644);
	if(fdwrite < 0)
	{
		printf("Failed to complete list operation\n");
		return;
	}
	while(currsize != tarsize)
	{
		lseek(fdtar, 18, SEEK_CUR);
		char filesize[11];
		if( read(fdtar, filesize, 10) < 0)
		{
			printf("Failed to complete list operation\n");
			return;
		}
		filesize[10] = '\0';
		int filesz = readint(filesize, 10);
		char *buff = (char *)malloc(1e10 * sizeof(char));
		if(read(fdtar, buff, filesz) < 0)
		{
			printf("Failed to complete list operation\n");
			return;
		}
		currsize += (28+filesz);
		files++;
	}
	char *temp1 = (char *) malloc(16 * sizeof(char));
	char *temp2 = (char *) malloc(6 * sizeof(char));
	sprintf(temp1,"%d", tarsize);

	if (write(fdwrite, temp1, strlen(temp1)) < 0)
	{
		printf("Failed to complete list operation\n");
		return;
	}

	sprintf(temp2,"%d", files);
	if (write(fdwrite, "\n", 1) < 0)
	{
		printf("Failed to complete list operation\n");
		return;
	}
	if(write(fdwrite, temp2, strlen(temp2)) < 0)
	{
		printf("Failed to complete list operation\n");
		return;
	}
	if(write(fdwrite, "\n", 1) < 0)
	{
		printf("Failed to complete list operation\n");
		return;
	}
	
	lseek(fdtar, 0, SEEK_SET);
	currsize = 0;

	while(currsize != tarsize)
	{
		char filenamesize[3];
		if(read(fdtar, filenamesize, 2) < 0)
		{
			printf("Failed to complete list operation\n");
			return;
		}
		filenamesize[2] = '\0';
		int fnsz = readint(filenamesize, 2);
		char fname[17];
		if(read(fdtar, fname, fnsz) < 0)
		{
			printf("Failed to complete list operation\n");
			return;
		}
		fname[fnsz] = '\0';
		char *t3 = (char *) malloc(17 * sizeof(char));
		//printf("%s ", fname);
		lseek(fdtar, 16-fnsz, SEEK_CUR);
		char filesize[11];
		if(read(fdtar, filesize, 10) < 0)
		{
			printf("Failed to complete list operation\n");
			return;
		}
		int filesz = readint(filesize, 10);
		//printf("%d\n", filesz);

		if(write(fdwrite, fname, strlen(fname)) < 0)
		{
			printf("Failed to complete list operation\n");
			return;
		}
		sprintf(t3,"%d", filesz);
		if(write(fdwrite, " ", 1) < 0)
		{
			printf("Failed to complete list operation\n");
			return;
		}
		if(write(fdwrite, t3, strlen(t3)) < 0)
		{
			printf("Failed to complete list operation\n");
			return;
		}
		if(write(fdwrite, "\n", 1) < 0)
		{
			printf("Failed to complete list operation\n");
			return;
		}

		char *buff = (char *) malloc ((1e10+1) * sizeof(char));
		if(read(fdtar, buff, filesz) < 0)
		{
			printf("Failed to complete list operation\n");
			return;
		}
		currsize += (28+filesz);
	}
}




int main(int argc, char *argv[])
{
	if(!strcmp(argv[1], "-c")) tar(argv[2], argv[3]);
	else if(!strcmp(argv[1], "-d")) untar(argv[2]);
	else if(!strcmp(argv[1], "-e")) untar_one(argv[2], argv[3]);
	else if(!strcmp(argv[1], "-l")) enlist(argv[2]);

	return 0;
}
