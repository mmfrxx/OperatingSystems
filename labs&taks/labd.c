#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

int insert_at(int fd, off_t offset, char *buf, int n) {
	int bufOrigSize = lseek(fd, 0L, SEEK_END) - offset;
	char *bufOrig = (char *) calloc(bufOrigSize, sizeof(char));

	lseek(fd, offset, SEEK_SET);
	if((read(fd, bufOrig, bufOrigSize)) == 0) {
		printf("Failed to read from destination file\n");
		exit(-1);
	}
	lseek(fd, offset, SEEK_SET);

	if((write(fd, buf, n)) == 0) {
		printf("Failed to write to file\n");
		exit(-1);
	}
	if((write(fd, bufOrig, bufOrigSize)) == 0) {
		printf("Failed to write to file\n");
		exit(-1);
	}
	free(bufOrig);
}

int copy_subtext(int fs, int fd, off_t offset, int num) {
	if(offset < 0) {
		printf("Offset cannot b  negative\n");
		exit(-1);
	}
	if(lseek(fs, 0L, SEEK_END) < offset + num) {
		printf("Not enough number of bytes in the source file\n");
		exit(-1);
	}
	lseek(fs, offset, SEEK_SET);
	
	char *buffer = (char *) calloc(num, sizeof(char));	
	
	if((read(fs, buffer, num)) == 0) {
		printf("Failed to read from source file\n");
		exit(-1);
	}
	
	insert_at(fd, offset, buffer, num);
	
	free(buffer);
	
	return lseek(fd, 0L, SEEK_END);
}

int main(int argc, char * argv[]) {
	if(argc != 5) {
		printf("Invalid number of arguments\n");
		exit(-1);
	}

	off_t offset = atoi(argv[3]);
	int num = atoi(argv[4]);

	int fs = open(argv[1], O_RDONLY);
	int fd;

	if(fs < 0) {
		printf("Failed to open file\n");
		exit(-1);
	}

	if((fd = open(argv[2], O_CREAT | O_RDWR, 0777)) == -1) {
		printf("Failed to open write file.\n");
		exit(-1);
	}

	int res = copy_subtext(fs, fd, offset, num);
	printf("The source file descriptor is %d\n", fs);
	printf("The destination file descriptor is %d\n", fd);
	printf("The file size is %d\n", res);

	close(fs);
	close(fd);
	return 0;
}
