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


int connectsock( char *host, char *service, char *protocol );

/*
**	Client
*/

void *consumer(void *servicesock){
    int 	 numberOfChars;
	char     *host = "localhost";
	int 	 csock;
	char	 *service = (char *)servicesock;
	int 	 cc;
	int 	 index = 0;


	if ( ( csock = connectsock( host, service, "tcp" )) == 0 ){
		fprintf( stderr, "Cannot connect to server in consumer.\n" );
	}
	else{
		write(csock, "CONSUME\r\n",9);
		fflush( stdout );

		if ((cc =  read(csock, &numberOfChars, 4)) <= 0 ){
			printf( "The server has gone.\n" );
			close(csock);
		}else{

		numberOfChars = htonl(numberOfChars);
		

		char *arrived = malloc(numberOfChars * sizeof(char*));

		while (( cc = read(csock, arrived + index, numberOfChars - index)) > 0 ){
			index += cc;
			if(index >= numberOfChars) break;
		}

		int id = pthread_self();
		char filename[45];
		sprintf(filename, "./text/%d.txt", id);

		int fd = open(filename,O_RDWR|O_CREAT, S_IRWXU);
		write(fd, arrived, numberOfChars);

		close(csock);
}
		pthread_exit(NULL);

	}
	
}

int
main( int argc, char *argv[] )
{
	char		buf[BUFSIZE];
	char		*service;		
	char		*host = "localhost";
	int			cc;
	int			csock;
	int         num;
	
	switch( argc ){
		case    3:
			service = argv[1];
			num = atoi(argv[2]);
			break;
		default:
			fprintf( stderr, "the wrong number of parameters\n" );
			exit(-1);
	}


	pthread_t threads[num];

	for( int i = 0; i < num; i++){
		int status = pthread_create(&threads[i], NULL, consumer, (void *) service);
		if(status != 0){
			printf("Error\n");
			exit(-1);
		}
	}

	for ( int j = 0; j < num; j++ )
		pthread_join( threads[j], NULL );

	pthread_exit(0);
}
