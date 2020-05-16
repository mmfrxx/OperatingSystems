#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>
#include <netinet/in.h>

#define BUFSIZE		4096

typedef struct item_t
{
	char *letters;
	int size;
} ITEM;


ITEM *makeItem()
{
	int i;
	ITEM *p = malloc( sizeof(ITEM) );
	p->size = random()%80;
	p->letters = malloc(p->size);	
	for ( i = 0; i < p->size-1; i++ )
		p->letters[i] = 'X';
	p->letters[i] = '\0';

	return p;
}


void *producer(void *servicesock){
    char		buf[BUFSIZE];
	int		    cc;

	char     *host = "localhost";
	int csock;
	char	 *service = (char *)servicesock;

	if ( ( csock = connectsock( host, service, "tcp" )) == 0 )
	{
		fprintf( stderr, "Cannot connect to server.\n" );
		exit( -1 );
	}
	
	write(csock, "PRODUCE\r\n\0",strlen("PRODUCE\r\n\0"));
	fflush( stdout );

    ITEM *item = makeItem();

    if ( (cc = read( csock, buf, BUFSIZE )) <= 0 ){
        printf( "The server has gone.\n" );
        close(csock);
    }else{
        buf[cc] = '\0';
        if(strcmp(buf,"GO\r\n\0") == 0){
            if ( write( csock, &item->size, 4 ) < 0 ){
                fprintf( stderr, "client write: %s\n", strerror(errno) );
                exit( -1 );
            }

            if ( write( csock, item->letters, strlen(*item->letters) ) < 0 ){
                fprintf( stderr, "client write: %s\n", strerror(errno) );
                exit( -1 );
            }
        } 
	}

    cc = read( csock, buf, BUFSIZE );
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
	char		*service;
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
		int status = pthread_create(&threads[i], NULL, producer, (void *) service);
		if(status != 0){
			printf("Error\n");
			exit(-1);
		}
	}
}

