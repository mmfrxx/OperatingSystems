#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

int insert_at(int fd, int offset,char *buff, int n){
	int size = lseek(fd, 0, SEEK_END);
	if(size < offset){
		printf("Size is less than offset");
		exit(0);
	}else if(size == 0){
		write(fd,buff, n);
		return 1;
	}
	
	char left[size-offset+1];

	lseek(fd, offset, SEEK_SET);
	int cur_offset = read(fd, left, size-offset);
	lseek(fd, offset, SEEK_SET);
	write(fd, buff, n);
	lseek(fd, offset + n, SEEK_SET);
	write(fd, left, size-offset);
	return 1;
}

int main(int argc, char *argv[]){
	if(argc != 5){
		printf("The number of arguments is invalid.");  
		exit(0);		
	}

	int offset = atoi(argv[2]);
	
	if(offset < 0){
		printf("OFFSET IS NEGATIVE!");
		return -1;
	}

	char *buf = argv[3];
	int n  = atoi(argv[4]);
	int fd = open(argv[1], O_RDWR| O_CREAT, S_IRWXU);
		
	if(fd < 0){
		printf("Invalid arguments.");
		exit(0);
	}
	
	int final_offset = insert_at(fd, offset, buf, n);
	close(fd);
	return 0;
}

