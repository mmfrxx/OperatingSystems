#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#define QLEN            5
#define BUFSIZE         1024
#define MAX_CLIENTS     512
#define MAX_PROD    480
#define MAX_CON     480

#define GO	        "GO\r\n"
#define CONSUME		"CONSUME\r\n"
#define PRODUCE		"PRODUCE\r\n"
#define DONE		"DONE\r\n"

#define MAX_LETTERS     1000000

int connectsock( char *host, char *service, char *protocol );
int passivesock( char *service, char *protocol, int qlen, int *rport );

typedef struct item_t
{
        int size;
        char *letters;
} ITEM;

ITEM 		**buffer;
int 		bufferIndex 	= 0;
int 		threadIndex 	= 0;
int 		clientsIndex 	= 0;
int 		consumerIndex 	= 0;
int 		producerIndex 	= 0;
int 		buf_size 		= 0;

pthread_mutex_t mutex1, mutex2;
sem_t full, empty;

void consume( int ssock ){
	int cc;
	int item_size;
	ITEM *p;
	
	
	sem_wait( &full );
	if( bufferIndex > 0 ) {
		pthread_mutex_lock( &mutex2 );
		bufferIndex--;	
		p = buffer[bufferIndex];
		buffer[bufferIndex] = NULL;

		printf("The number of ITEM in the buffer: %d\n", bufferIndex);
		fflush(stdout);
		pthread_mutex_unlock( &mutex2 );
		
		sem_post( &empty );

		item_size = htonl(p->size);
		
		write( ssock, &item_size, 4 );

		write( ssock, p->letters, p->size );

		free( p->letters);
		free(p);
	}

	pthread_mutex_lock( &mutex1 );
	clientsIndex--;
	consumerIndex--;
	pthread_mutex_unlock( &mutex1 );
	close( ssock );	
	pthread_exit(0);
}

void produce( int ssock ){
	int cc;
	int item_size;
	int letter_index;

	sem_wait(&empty);
	//if( bufferIndex < buf_size ){
		write(ssock, GO, 4);
		if( (cc = read( ssock, &item_size, 4 )) <= 0 ) 
			printf( "The server has gone.\n" );
		else item_size = htonl(item_size);	

		ITEM *p = malloc( sizeof(ITEM) );
		p->size = item_size;
		p->letters = malloc( (item_size) * sizeof(char) );	
			
		while( cc != 0 ) {
			if( letter_index >= item_size ) 
				break;
			cc = read( ssock, p->letters+letter_index, item_size-letter_index );
			letter_index = letter_index + cc;
		}
		
		if( letter_index < item_size ){
			pthread_mutex_lock( &mutex1 );
			clientsIndex--;
			producerIndex--;
			pthread_mutex_unlock( &mutex1 );
			sem_post(&empty);
		}
		else {
			pthread_mutex_lock( &mutex2 );
			buffer[bufferIndex] = p;
			
			printf("buffer index(p): %d\n", bufferIndex);
			fflush(stdout);
			bufferIndex++;

			pthread_mutex_unlock( &mutex2 );
				
			write( ssock, DONE, 6 );

			pthread_mutex_lock( &mutex1 );
			clientsIndex--;
			producerIndex--;
			pthread_mutex_unlock( &mutex1 );
			sem_post(&full);
		}
		
	//}
	
	close( ssock );
	pthread_exit(0);
}

void *echo( void *sock ){
	int 	ssock = (int)( sock );
	int 	cc;
	char 	buf[BUFSIZE];

	if ( (cc = read( ssock, buf, BUFSIZE )) <= 0 ){
		printf( "Thread for socket %d is gone\n", ssock );
		printf( "The consumer/producer has gone.\n" );
		close( ssock );
	}else{
		buf[cc] = '\0';

		if ( strcmp( buf, PRODUCE ) == 0 ) {
			pthread_mutex_lock( &mutex1 );
			if( producerIndex < MAX_PROD ) {
				producerIndex++;
				pthread_mutex_unlock( &mutex1 );
				produce( ssock );
			}else{
				clientsIndex--;
				pthread_mutex_unlock( &mutex1 );
				printf("There are too many producers... closing\n\r");
			}
		}

		else if( strcmp( buf, CONSUME ) == 0 ){
			pthread_mutex_lock( &mutex1 );
			if( consumerIndex < MAX_CON ){
				consumerIndex++;
				pthread_mutex_unlock( &mutex1 );
				consume( ssock );
			}else{
				clientsIndex--;
				pthread_mutex_unlock( &mutex1 );
				printf("There are too many concumers... closing...\n\r");
			}
		}		
	}
	close( ssock );
}


int main( int argc, char *argv[] ){
	struct sockaddr_in	fsin;
	char				*service;
	int					alen;
	int					msock;
	int					rport = 0;

	pthread_mutex_init( &mutex1, NULL );
	pthread_mutex_init( &mutex2, NULL );

	
	switch (argc){
		case	2:
			// No args? let the OS choose a port and tell the user
			rport = 1;
			buf_size = atoi(argv[1]);
			break;
		case	3:
			// User provides a port? then use it
			service = argv[1];
			buf_size = atoi(argv[2]);
			break;
		default:
			fprintf( stderr, "usage: server [port]\n" );
			exit( -1 );
	}
	
	buffer = malloc( buf_size * sizeof( ITEM* ) );

	sem_init( &full, 0, 0 );
	sem_init( &empty, 0, buf_size );

	msock = passivesock( service, "tcp", QLEN, &rport );
	if (rport){
		printf( "server: port %d\n", rport );	
		fflush( stdout );
	}

	for(;;){
		if( buf_size > 0 ){
			int	ssock;
			pthread_t	thr;
			
			alen = sizeof(fsin);
			ssock = accept( msock, (struct sockaddr *)&fsin, &alen );

			if (ssock < 0){
				fprintf( stderr, "accept: %s\n", strerror(errno) );
				break;
			}
			 
			pthread_mutex_lock( &mutex1 );
			if( clientsIndex < MAX_CLIENTS ){
				clientsIndex++;
				pthread_mutex_unlock( &mutex1 );
				pthread_create( &thr, NULL, echo, (void *) ssock );
			} else {
				pthread_mutex_unlock( &mutex1 );
				printf("The clients are exceeded the maximum.\n\n\n\n\n");
				fflush(stdout);
				close( ssock );
			}
		}else break;
	}

	exit(0);
}
