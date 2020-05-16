#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]){
	if(argc != 4){
		printf("The number of arguments is invalid.");  
		exit(0);		
	}
	char *fileName =  (char *)malloc(strlen(argv[1]));
	strcpy(fileName, argv[1]);
	int offset = *argv[2];
	char *str = (char *)malloc(strlen(argv[3]));
	strcpy(str, argv[3]);

	int fp = open(fileName, O_WRONLY | O_RDONLY);
	
	free(fileName);
	free(str);
	if(fp < 0){
		printf("Invalid arguments.");
		exit(0);
	}
	
	printf("%i", fp);
	close(fp);
	return 0;
}
	
