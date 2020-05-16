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
#include <fcntl.h>
#include <sys/stat.h>

#define BUFSIZE		4096

void *consumer(void *servicesock){
    int		 numberOfChars;		
	char     *host = "localhost";
	int csock;
	char	 *service = (char *)servicesock;


	if ( ( csock = connectsock( host, service, "tcp" )) == 0 ){
		fprintf( stderr, "Cannot connect to server.\n" );
		exit( -1 );
	}
	
	write(csock, "CONSUME\r\n\0",strlen("CONSUME\r\n\0"));
	fflush( stdout );

    if ( read(csock, &numberOfChars, 4) <= 0 ){
        printf( "The server has gone.\n" );
        close(csock);
        exit(0);
    }

	htonl(numberOfChars);

    char arrived[numberOfChars];

    if ( read(csock, arrived, numberOfChars) <= 0 ){
        printf( "The server has gone.\n" );
        close(csock);
        exit(0);
    }

    int id = pthread_self();
    char filename[45];
    sprintf(filename, "%d.txt\0", id);

    int fd = open(filename,O_RDWR|O_CREAT, S_IRWXU);
    write(fd, arrived, numberOfChars);

	close(csock);

	pthread_exit(NULL);
}

int connectsock( char *host, char *service, char *protocol );

/*
**	Client
*/
int
main( int argc, char *argv[] )
{
	char		buf[BUFSIZE];
	char		*service;
	int			cc;
	int         num;
	
	switch( argc ){
		case    3:
			service = argv[1];
			num = argv[2];
			break;
		default:
			fprintf( stderr, "the wrong number of parameters\n" );
			exit(-1);
	}

	

	pthread_t threads[num];

	for(int i = 0; i < num; i++){
		int status = pthread_create(&threads[i], NULL, consumer, (void *) service);
		if(status != 0){
			printf("Error\n");
			exit(-1);
		}
	}
}

