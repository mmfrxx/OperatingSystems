#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <semaphore.h>
#include <prodcon.h>


#define GO	        "GO\r\n"
#define CONSUME		"CONSUME\r\n"
#define PRODUCE		"PRODUCE\r\n"
#define DONE		"DONE\r\n"


ITEM 		**buffer;
int 		bufferIndex 	= 0;
int 		threadIndex 	= 0;
int 		clientsIndex 	= 0;
int 		consumerIndex 	= 0;
int 		producerIndex 	= 0;
int 		buf_size 		= 10;

pthread_mutex_t mutex1, mutex2;
sem_t full, empty;


/*
**	The server ... uses multiplexng to switch between clients
**	Each client gets one echo per turn, 
**	but can have as many echoes as it wants until it disconnects
*/

void *produce( void *sock ){
	int 	ssock = (int)( sock );
	int cc;
	int item_size;
	int letter_index;

	sem_wait(&empty);
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
		
		
		printf("The number of ITEM in the buffer: %d\n", bufferIndex + 1);
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
	
	close( ssock );
	pthread_exit(0);
}


void *consume( void *sock ){
	int 	ssock = (int)( sock );
	int cc;
	int item_size;
	ITEM *p;
	
	
	sem_wait( &full );
	
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


	pthread_mutex_lock( &mutex1 );
	clientsIndex--;
	consumerIndex--;
	pthread_mutex_unlock( &mutex1 );
	close( ssock );	
	pthread_exit(0);
}


int
main( int argc, char *argv[] )
{
	char			buf[BUFSIZE];
	char			*service;
	struct sockaddr_in	fsin;
	int			msock;
	int			ssock;
	fd_set			rfds;
	fd_set			afds;
	int			alen;
	int			fd;
	int			nfds;
	int			rport = 0;
	int			cc;

	pthread_mutex_init( &mutex1, NULL );
	pthread_mutex_init( &mutex2, NULL );
	
	switch (argc){
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
			exit( -1 );
	}
	
	buffer = malloc( buf_size * sizeof( ITEM* ) );

	sem_init( &full, 0, 0 );
	sem_init( &empty, 0, buf_size );

	// Create the main socket as usual
	// This is the socket for accepting new clients
	msock = passivesock( service, "tcp", QLEN, &rport );
	if (rport)
	{
		//	Tell the user the selected port
		printf( "server: port %d\n", rport );	
		fflush( stdout );
	}

	// Now we begin the set up for using select
	
	// nfds is the largest monitored socket + 1
	// there is only one socket, msock, so nfds is msock +1
	// Set the max file descriptor being monitored
	nfds = msock+1;

	// the variable afds is the fd_set of sockets that we want monitored for
	// a read activity
	// We initialize it to empty
	FD_ZERO(&afds);
	
	// Then we put msock in the set
	FD_SET( msock, &afds );

	// Now start the loop
	for (;;)
	{
		// Since select overwrites the fd_set that we send it, 
		// we copy afds into another variable, rfds
		// Reset the file descriptors you are interested in
		memcpy((char *)&rfds, (char *)&afds, sizeof(rfds));

		// Only waiting for sockets who are ready to read
		//  - this includes new clients arriving
		//  - this also includes the client closed the socket event
		// We pass null for the write event and exception event fd_sets
		// we pass null for the timer, because we don't want to wake early
		if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0,
				(struct timeval *)0) < 0)
		{
			fprintf( stderr, "server select: %s\n", strerror(errno) );
			exit(-1);
		}

		// Since we've reached here it means one or more of our sockets has something
		// that is ready to read
		// So now we have to check all the sockets in the rfds set which select uses
		// to return a;; the sockets that are ready

		// If the main socket is ready - it means a new client has arrived
		// It must be checked separately, because the action is different
		if (FD_ISSET( msock, &rfds)) 
		{
			int	ssock;

			// we can call accept with no fear of blocking
			alen = sizeof(fsin);
			ssock = accept( msock, (struct sockaddr *)&fsin, &alen );
			if (ssock < 0)
			{
				fprintf( stderr, "accept: %s\n", strerror(errno) );
				exit(-1);
			}


			pthread_mutex_lock( &mutex1 );
			if( clientsIndex < MAX_CLIENTS ){
				clientsIndex++;
				pthread_mutex_unlock( &mutex1 );
			} else {
				pthread_mutex_unlock( &mutex1 );
				printf("The clients exceeded the maximum.\n\n\n\n\n");
				fflush(stdout);
				close( ssock );
				continue;
			}





			// If a new client arrives, we must add it to our afds set
			FD_SET( ssock, &afds );

			// and increase the maximum, if necessary
			if ( ssock+1 > nfds )
				nfds = ssock+1;
		}

		// Now check all the regular sockets
		for ( fd = 0; fd < nfds; fd++ )
		{
			// check every socket to see if it's in the ready set
			// But don't recheck the main socket
			if (fd != msock && FD_ISSET(fd, &rfds))
			{
				// you can read without blocking because data is there
				// the OS has confirmed this
				if ( (cc = read( fd, buf, BUFSIZE )) <= 0 )
				{
					printf( "The client has gone.\n" );
					(void) close(fd);
				}
				else
				{
					// Otherwise send the echo to the client
                    
			        pthread_t	thr;
                    buf[cc] = '\0';

                    if ( strcmp( buf, PRODUCE ) == 0 ) {
						pthread_mutex_lock( &mutex1 );
						if( producerIndex < MAX_PROD ) {
							producerIndex++;
							pthread_mutex_unlock( &mutex1 );
							pthread_create(&thr, NULL,produce,(void *)fd);
						}else{
							clientsIndex--;
							pthread_mutex_unlock( &mutex1 );
							printf("There are too many producers... closing\n\r");
							fflush(stdout);
							close(fd);
						}
                       
                    }

                    else if( strcmp( buf, CONSUME ) == 0 ){
						pthread_mutex_lock( &mutex1 );
						if( consumerIndex < MAX_CON ){
							consumerIndex++;
							pthread_mutex_unlock( &mutex1 );
                        	pthread_create(&thr, NULL,consume,(void *)fd);
						}else{
							clientsIndex--;
							pthread_mutex_unlock( &mutex1 );
							printf("There are too many concumers... closing...\n\r");
							fflush(stdout);
							close(fd);
						}
                    }
				}
                // If the client has closed the connection, we need to
				// stop monitoring the socket (remove from afds)
				FD_CLR( fd, &afds );
				// lower the max socket number if needed
				if ( nfds == fd+1 )
					nfds--;
			}

		}
	}
}

