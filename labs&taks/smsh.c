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

#define CMDLEN 80

int main()
{
	int pid;
	int status;
	int i, fd;
	char command[254];

	printf( "Program begins.\n" );
	
	for (;;)
	{
		printf( "smsh%   " );
		fgets( command, CMDLEN, stdin );
		command[strlen(command)-1] = '\0';
		if ( strcmp(command, "quit") == 0 )
			break;
		
		char *token = strtok(command, " ");
		int num_of_tokens = 0;
		char *pointers[100];
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
			printf( "PARENT. pid = %d, mypid = %d.\n", pid, getpid() );
			waitpid( pid, &status, 0 );
		}
		else
		{
			printf( "CHILD. pid = %d, mypid = %d.\n", pid, getpid() );
			execvp( command, pointers);
			break;

		}
	}
}
