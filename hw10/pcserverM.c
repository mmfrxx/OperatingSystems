#include "prodcon.h"

#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

int             icount, prod_count, con_count, client_count;
ITEM**          ibuffer;
pthread_mutex_t pcmutex, mtmutex;
sem_t           full, empty;

int main( int argc, char** argv )

{
	char*              err;
	socklen_t          alen;
	int                rport;
	int                maxitems;
	int                msock;
	int                ssock;
	pthread_t          thread;
	char*              service;
	struct sockaddr_in fsin;

	rport    = 0;
	maxitems = 0;
	if ( argc == 2 ) {
		maxitems = atoi( *( char** ) ( argv + 8 ) );
		rport    = 1;
	} else {
		if ( argc != 3 ) {
			fprintf( stderr, "usage: pcserver [port] maxitems\n" );
			exit( -1 );
		}
		service  = argv[ 1 ];
		maxitems = atoi( argv[ 2 ] );
	}
	if ( 0 < maxitems ) {
		ibuffer = malloc( maxitems * sizeof( ITEM* ) );
		pthread_mutex_init( &pcmutex, 0 );
		pthread_mutex_init( &mtmutex, 0 );
		sem_init( &full, 0, 0 );
		sem_init( &empty, 0, maxitems );
		icount       = 0;
		client_count = 0;
		con_count    = 0;
		prod_count   = 0;
		msock        = passivesock( service, "tcp", QLEN, &rport );
		if ( rport != 0 ) {
			fprintf( stderr, "pcserver: port is %d\n", rport );
			fflush( stdout );
		}
		while ( 1 ) {
			alen  = sizeof( fsin );
			ssock = accept( msock, &fsin, &alen );
			if ( ssock < 0 )
				break;
			puts( "A client has arrived." );
			fflush( stdout );
			pthread_mutex_lock( &mtmutex );
			if ( client_count < MAX_CLIENTS ) {
				client_count = client_count + 1;
				pthread_create( &thread, 0x0, serve, ( void* ) &ssock );
			} else {
				close( ssock );
				fprinf( stderr, "REJECTED\n" );
			}
			pthread_mutex_unlock( &mtmutex );
		}
		err = strerror( errno );
		fprintf( stderr, "accept: %s\n", err );
		pthread_exit( NULL );
	}
	fprintf( stderr, "pcserver: maxitems must be > 0\n" );
	exit( -1 );
}


void close_socket( char msg, int sock, int type )

{
	if ( msg != 0 ) {
		printf( "pcserver: %s\n", msg );
	}
	pthread_mutex_lock( &mtmutex );
	client_count = client_count - 1;
	if ( type == 0 ) {
		prod_count = prod_count - 1;
	}
	if ( type == 1 ) {
		con_count = con_count - 1;
	}
	pthread_mutex_unlock( &mtmutex );
	close( sock );
	pthread_exit( NULL );
}


void* serve( void* sock )

{
	bool    is_free;
	ssize_t rb;
	char    buf[ 1032 ];

	rb = read( *( int* ) sock, buf, 0x400 );
	if ( ( int ) rb < 1 ) {
		close_socket( "The client has gone unexpectedly", *( int* ) sock, 2 );
	} else {
		buf[ ( int ) rb ] = '\0';
		if ( strcmp( buf, "PRODUCE\r\n" ) == 0 ) {
			pthread_mutex_lock( &mtmutex );
			is_free = prod_count < MAX_PROD;
			if ( is_free ) {
				prod_count = prod_count + 1;
			}
			pthread_mutex_unlock( &mtmutex );
			if ( is_free ) {
				produce( *( int* ) sock );
				pthread_exit( NULL );
			}
			close_socket( "REJECTED\n", *( int* ) sock, 2 );
		} else {
			if ( strcmp( buf, "CONSUME\r\n" ) == 0 ) {
				pthread_mutex_lock( &mtmutex );
				is_free = con_count < MAX_CON;
				if ( is_free ) {
					con_count = con_count + 1;
				}
				pthread_mutex_unlock( &mtmutex );
				if ( is_free ) {
					consume( *( int* ) sock );
					pthread_exit( NULL );
				}
				close_socket( "REJECTED\n", *( int* ) sock, 2 );
			} else {
				close_socket( "unrecognized command - closing", *( int* ) sock, 2 );
			}
		}
	}
}


void produce( int sock )

{
	uint32_t received_len;
	int      str_index;
	int      rb;
	ITEM*    received_item;

	str_index     = 0;
	received_item = ( uint32_t* ) malloc( 0x10 );
	write( sock, "GO\r\n", 4 );
	read( sock, &received_len, 4 );
	received_len        = ntohl( received_len );
	received_item->size = received_len;
	printf( "The len is %d\n", received_len );
	received_item->letters = malloc( ( received_len + 1 ) * sizeof( char ) );
	do {
		if ( received_len <= str_index )
			break;
		rb        = read( sock, ( void* ) ( received_item->letters + str_index ),
                   received_len - str_index );
		str_index = str_index + rb;
	} while ( rb != 0 );
	if ( str_index < received_len ) {
		close_socket( "The client has gone unexpectedly", sock, 0 );
	} else {
		sem_wait( &empty );
		pthread_mutex_lock( &pcmutex );
		ibuffer[ icount ] = received_item;
		icount            = icount + 1;
		printf( "C Count %d.\n", icount );
		pthread_mutex_unlock( &pcmutex );
		received_item->letters[ received_item->size ] = 0;
		sem_post( &full );
		write( sock, "DONE\r\n", 6 );
		close_socket( "The produced item was added with success", sock, 0 );
	}
}


void consume( int sock )

{
	uint32_t item_size;
	ITEM*    sent_item;

	sem_wait( &full );
	pthread_mutex_lock( &pcmutex );
	sent_item             = ibuffer[ icount - 1 ];
	ibuffer[ icount - 1 ] = NULL;
	icount                = icount - 1;
	printf( "C Count %d.\n", icount );
	pthread_mutex_unlock( &pcmutex );
	sem_post( &empty );
	item_size = htonl( sent_item->size );
	write( sock, &item_size, 4 );
	write( sock, sent_item->letters, sent_item->size );
	free( sent_item->letters );
	free( sent_item );
	close_socket( "Successfully sent the item to consumer.\n", sock, 1 );
}