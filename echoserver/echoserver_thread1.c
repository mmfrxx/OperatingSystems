#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>

#define	QLEN			5
#define	BUFSIZE			4096

void *work(void *arg){
	printf("created thread");
	
	char			buf[BUFSIZE];
	int			cc;
	int *ssock = (int *)arg;

	for (;;)
	{
		if ( (cc = read( ssock, buf, BUFSIZE )) <= 0 )
		{
			printf( "The client has gone.\n" );
			close(ssock);
			break;
		}
		else
		{
			buf[cc] = '\0';
			printf( "The client says: %s\n", buf );
			if ( write( ssock, buf, cc ) < 0 )
			{
				/* This guy is dead */
				close( ssock );
				break;
			}
		}
	}
	pthread_exit(0);
}

int passivesock( char *service, char *protocol, int qlen, int *rport );

/*
**	This poor server ... only serves one client at a time
*/

int
main( int argc, char *argv[] )
{
	char			*service;
	struct sockaddr_in	fsin;
	socklen_t		alen;
	int			msock;
	int			ssock;
	int			rport = 0;
	
	switch (argc) 
	{
		case	1:
			// No args? let the OS choose a port and tell the user
			rport = 1;
			break;
		case	2:
			// User provides a port? then use it
			service = argv[1];
			break;
		default:
			fprintf( stderr, "usage: server [port]\n" );
			exit(-1);
	}

	msock = passivesock( service, "tcp", QLEN, &rport );
	if (rport)
	{
		//	Tell the user the selected port
		printf( "server: port %d\n", rport );	
		fflush( stdout );
	}
	
	pthread_t threads[1024];
	int i = 0;
	
	for (;;)
	{
		int	*ssock = (int *) malloc(sizeof(int));

		alen = sizeof(fsin);
		*ssock = accept( msock, (struct sockaddr *)&fsin, &alen );
		if (ssock < 0)
		{
			fprintf( stderr, "accept: %s\n", strerror(errno) );
			exit(-1);
		}

		printf( "A client has arrived for echoes.\n" );
		fflush( stdout );

		/* start working for this guy */
		/* ECHO what the client says */

		int status = pthread_create(&threads[i++], NULL, work, (void *)&ssock);
		if(status != 0) exit(-1);

	}
	pthread_exit(0);
}


