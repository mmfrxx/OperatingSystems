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
#include <semaphore.h>

#include <prodcon.h>

ITEM 		**buffer;
int 		buf_idx 		= 0;
int 		thread_idx 		= 0;
int 		clients_idx 	= 0;
int 		consumer_idx 	= 0;
int 		producer_idx 	= 0;
int 		buf_size 		= 0;

pthread_mutex_t mutex1, mutex2;
sem_t full, empty;

void consume( int ssock ) 
{
	int cc;
	int item_size;
	ITEM *p;
	
	// lock
	sem_wait( &full );
	if( buf_idx > 0 ) {
		pthread_mutex_lock( &mutex2 );	

		p = buffer[buf_idx-1];
		buffer[buf_idx-1] = NULL;
		buf_idx--;


		printf("buffer index(c): %d\n", buf_idx);
		fflush(stdout);

		pthread_mutex_unlock( &mutex2 );

		item_size = htonl(p->size);
		
		write( ssock, &item_size, 4 );

		write( ssock, p->letters, p->size );

		free( p->letters );
		free( p );
	}
	
	sem_post( &empty );
	pthread_mutex_lock( &mutex1 );
	clients_idx--;
	consumer_idx--;
	pthread_mutex_unlock( &mutex1 );
	close( ssock );
	pthread_exit(0);	
}

void produce( int ssock ) 
{
	int cc = -1;
	int item_size;
	int letter_idx;

	sem_wait(&empty);
	write(ssock, GO, 4);
	//else 
	//{
		if( (cc = read( ssock, &item_size, 4 )) <= 0 ) 
			printf( "The server has gone.\n" );

		else item_size = htonl(item_size);	

		ITEM *p = malloc( sizeof(ITEM) );
		p->size = item_size;
		p->letters = malloc( (item_size) * sizeof(char) );	

		
		while( cc != 0 ) 
		{
			if( letter_idx >= item_size ) break;
			cc = read( ssock, p->letters+letter_idx, item_size-letter_idx );
			letter_idx = letter_idx + cc;
		}
		
		if( letter_idx < item_size )
		{
			pthread_mutex_lock( &mutex1 );
			clients_idx--;
			producer_idx--;
			pthread_mutex_unlock( &mutex1 );
			close( ssock );
			sem_post(&empty);
		}
		else 
		{
			pthread_mutex_lock( &mutex2 );
			buffer[buf_idx] = p;
			
			printf("buffer index(p): %d\n", buf_idx);
			fflush(stdout);

			buf_idx++;
			pthread_mutex_unlock( &mutex2 );
		
			write( ssock, DONE, 6 );

			pthread_mutex_lock( &mutex1 );
			clients_idx--;
			producer_idx--;
			pthread_mutex_unlock( &mutex1 );
			close( ssock );
			sem_post(&full);
		}
	//}
	pthread_exit(0);
}

void *echo( void *sock ) 
{
	int 	ssock = (int)( sock );
	int 	cc;
	char 	buf[BUFSIZE];

	if ( (cc = read( ssock, buf, BUFSIZE )) <= 0 )
	{
		printf( "Thread for socket %d is gone\n", ssock );
		printf( "The consumer/producer has gone.\n" );
		close( ssock );
	} 
	else
	{
		buf[cc] = '\0';
		
		if ( strcmp( buf, PRODUCE ) == 0 ) 
		{
			pthread_mutex_lock( &mutex1 );
			if( producer_idx < MAX_PROD ) {
				producer_idx++;
				pthread_mutex_unlock( &mutex1 );
				produce( ssock );
			} else {
				// pthread_mutex_lock( &mutex1 );
				clients_idx--;
				pthread_mutex_unlock( &mutex1 );
			}
		}
		else if( strcmp( buf, CONSUME ) == 0 )
		{
			
			pthread_mutex_lock( &mutex1 );
			if( consumer_idx < MAX_CON ) {
				consumer_idx++;
				pthread_mutex_unlock( &mutex1 );
				consume( ssock );
			} else {
				// pthread_mutex_lock( &mutex1 );
				clients_idx--;
				pthread_mutex_unlock( &mutex1 );
			}
		}		
	}

	close( ssock );
}


int main( int argc, char *argv[] )
{
	struct sockaddr_in	fsin;
	char				*service;
	int					alen;
	int					msock;
	int					rport = 0;

	pthread_mutex_init( &mutex1, 0 );
	pthread_mutex_init( &mutex2, 0 );

	switch (argc) 
	{
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
	if (rport) 
	{
		printf( "server: port %d\n", rport );	
		fflush( stdout );
	}

	for(;;) 
	{
		if( buf_size > 0 ) 
		{
			int	ssock;
			pthread_t	thr;
			
			alen = sizeof(fsin);
			ssock = accept( msock, (struct sockaddr *)&fsin, &alen );

			if (ssock < 0)
			{
				fprintf( stderr, "accept: %s\n", strerror(errno) );
				break;
			}

			// lock 
			pthread_mutex_lock( &mutex1 );
			if( clients_idx < MAX_CLIENTS )	
			{
				clients_idx++;
				pthread_mutex_unlock( &mutex1 );
				pthread_create( &thr, NULL, echo, (void *) ssock );
			} 
			else 
			{
				pthread_mutex_unlock( &mutex1 );
				close( ssock );
			}
		}
	}

	pthread_exit( 0 );
	exit( 0 );
}
