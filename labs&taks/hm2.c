/*
**	This program is a very simple shell that only handles 
**	single word commands (no arguments).
**	Type "quit" to quit.
*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define CMDLEN 254
int main()
{
	int pid;
	int status;
	int fd;
	char command[254];
	int saved_stdout = dup(1);

	printf( "Program begins.\n" );
	
	for (;;){
		dup2(saved_stdout, 1);
		int num_of_tokens = 0;
		char *pointers[100];
		int back = 0;

		printf( "smsh%% " );
		fgets( command, CMDLEN, stdin );
		command[strlen(command)-1] = '\0';
		
		if ( strcmp(command, "quit") == 0 ){
			close(saved_stdout);
			break;
		}
		
		if(command[strlen(command)-1] == '&'){
			back = 1;
			command[strlen(command) - 1] = '\0';
		}
		
		char *token = strtok(command, " ");
		while(token!= NULL && num_of_tokens < 100){
			pointers[num_of_tokens] = token;
			num_of_tokens++;
			token = strtok(NULL, " ");
		}

		if(num_of_tokens == 100)
			pointers[99] = NULL;
		else
			pointers[num_of_tokens] = NULL;

		pid = fork();
		if ( pid < 0 )
		{
			printf( "Error in fork.\n" );
			exit(-1);
		}
		if ( pid != 0 )
		{
			printf( "\n\n\n\nPARENT. pid = %d, mypid = %d.\n", pid, getpid() );
			if(back == 0)
				waitpid( pid, &status, 0 );
		}
		else
		{
			printf( "\n\nCHILD. pid = %d, mypid = %d.\n", pid, getpid() );
			if(num_of_tokens > 2){
				if( strcmp(pointers[num_of_tokens - 2],">") == 0  ){
					fd = open(pointers[num_of_tokens - 1], O_RDWR | O_CREAT | O_TRUNC, S_IRWXU );
				}else if(strcmp(pointers[num_of_tokens - 2],">>") == 0){
					fd = open(pointers[num_of_tokens - 1], O_RDWR | O_CREAT, S_IRWXU );	
					lseek(fd, 0, SEEK_END);
				}
				
				if(strcmp(pointers[num_of_tokens - 2],">>") == 0 || strcmp(pointers[num_of_tokens - 2],">") == 0){
					if(fd < 0){
						printf("Problem arised with >> .");
						exit(-1);
					}
					dup2(fd, 1);
					close(fd);
					pointers[num_of_tokens - 1] = NULL;
					pointers[num_of_tokens - 2] = NULL; 
				}
			}
			execvp( command, pointers);
			break;
		}
	}
}
