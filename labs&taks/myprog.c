#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

int replace_at(int s, int d, int offset, int n);

int main(int argc, char *argv[]){
	if(argc != 5){
		printf("The number of arguments is invalid.");  
		exit(0);		
	}
	char *source =  (char *)malloc(strlen(argv[1]));
	strcpy(source, argv[1]);
	char *dest = (char *)malloc(strlen(argv[2]));
	strcpy(dest, argv[2]);
	int offset = *argv[3];
	int n  = *argv[4];

	int s = open(source, O_RDWR);
        int d = open(dest, O_RDWR|O_CREAT, S_IRWXU);
		
	free(source);
	free(dest);
	if(s < 0){
		printf("Invalid arguments.");
		exit(0);
	}
	


	int final_offset = replace_at(s, d, offset, n);
	printf("The current offset is at %i", final_offset);
	close(s);
	close(d);
	return 0;
}

int replace_at(int s, int d, int offset, int n){
	lseek(s, offset, SEEK_SET);
	lseek(d, offset, SEEK_SET);
	char buf[n];
	n = read(s, buf,n);
	write(d, buf, n);
	return lseek(s, 0 , SEEK_CUR);
	
}
	
