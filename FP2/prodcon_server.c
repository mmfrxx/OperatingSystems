#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
//#include <sys/time.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <semaphore.h>
#include <prodcon.h>
#include <time.h>

#define GO	        "GO\r\n"
#define CONSUME		"CONSUME\r\n"
#define PRODUCE		"PRODUCE\r\n"
#define DONE		"DONE\r\n"
#define STATUS      "STATUS\0"

ITEM 		**buffer;
int 		bufferIndex 	= 0;
int 		threadIndex 	= 0;
int 		clientsIndex 	= 0;
int 		consumerIndex 	= 0;
int 		producerIndex 	= 0;
int 		buf_size;
int         prodServed      = 0;
int         conServed       = 0;
int         badReject       = 0;
int         limitReject     = 0;
int         limitProdReject = 0;
int         limitConReject  = 0;
time_t      arrivalTime[512];      



pthread_mutex_t mutex1, mutex2;
sem_t full, empty;

int min(int a, int b){
	if(a > b) return b;
	else return a;
}

char *substring(char *string, int position, int length)
{
   char *pointer;
   int c;
 
   pointer = malloc(length);
   
   for (c = 0 ; c < length ; c++)
   {
      *(pointer+c) = *(string+position);      
      string++;  
   }
 
   return pointer;
}


void *produce( void *sock ){
	int 	ssock = (int)( sock );
	int cc;
	int item_size;
	int letter_index;

	
	write(ssock, GO, 4);
	
	if( (cc = read( ssock, &item_size, 4 )) <= 0 ){
		printf( "The producer has gone.\n" );
		pthread_mutex_lock(&mutex1);
			clientsIndex--;
			producerIndex--;
		pthread_mutex_unlock(&mutex1);
		close(ssock);
	}
	else {
		ITEM *p = malloc( sizeof(ITEM) );
		p->size = htonl(item_size);	
		p->psd = ssock;

		sem_wait(&empty);
		pthread_mutex_lock( &mutex2 );
		buffer[bufferIndex] = p;
		printf("The number of ITEM in the buffer (prod thread): %d\n", bufferIndex + 1);
		bufferIndex++;
		fflush(stdout);
		prodServed++;
		pthread_mutex_unlock( &mutex2 );	
		sem_post(&full);
	}
	
	pthread_exit(0);
}

void *consume( void *sock ){
	int     ssock = (int)( sock );
	int 				       cc;
	int 				item_size;
	ITEM					   *p;
	int 			       r_sock;
	char	 *arrived = malloc(BUFSIZE * sizeof(char*));
	int                   idx = 0;
	
	sem_wait( &full );
	pthread_mutex_lock( &mutex2 );
	bufferIndex--;
	p = buffer[bufferIndex];
	buffer[bufferIndex] = NULL;
	conServed++;
	printf("The number of ITEM in the buffer (con thread): %d\n", bufferIndex);
	fflush(stdout);
	pthread_mutex_unlock( &mutex2 );
	sem_post( &empty );

	item_size = p->size;
	r_sock = p->psd;
	int reverse = htonl(item_size);
	write( ssock, &reverse, 4 );	
	write(r_sock, GO, 4);
	
	while (( cc = read(r_sock, arrived, min(BUFSIZE, item_size - idx))) > 0 ){
		idx = idx + cc;
		write(ssock,arrived,cc);
		if(idx >= item_size) break;
	}
	
	if( idx < item_size ){
		printf("Something went seriously wrong \
		... the sizes are not equal!\n\r");
		fflush(stdout);
	}else{
		write( r_sock, DONE, 6 );
	}
	
	close( ssock );	
	close(r_sock);
	free(arrived);
	free(p);
	pthread_mutex_lock(&mutex1);
	clientsIndex = clientsIndex - 2;
	consumerIndex--;
	producerIndex--;
	pthread_mutex_unlock(&mutex1);
	pthread_exit(0);
}

void manageStatus(int fd,char buff[BUFSIZE]){
	char response[50];

	if(strcmp(buff,"STATUS/CURRCLI\r\n") == 0){
		sprintf(response, "%d\r\n", clientsIndex);
		write(fd, response, strlen(response));
	}
	else if(strcmp(buff,"STATUS/CURRPROD\r\n") == 0){
		sprintf(response, "%d\r\n", producerIndex);
		write(fd, response, strlen(response));
	}
	else if(strcmp(buff,"STATUS/CURRCONS\r\n") == 0){
		sprintf(response, "%d\r\n", consumerIndex);
		write(fd, response, strlen(response));
	}
	else if(strcmp(buff,"STATUS/TOTPROD\r\n") == 0){
		sprintf(response, "%d\r\n", prodServed);
		write(fd, response, strlen(response));
	}
	else if(strcmp(buff,"STATUS/TOTCONS\r\n") == 0){
		sprintf(response, "%d\r\n", conServed);
		write(fd, response, strlen(response));
	}
	else if(strcmp(buff,"STATUS/REJMAX\r\n") == 0){
		sprintf(response, "%d\r\n", limitReject);
		write(fd, response, strlen(response));
	}
	else if(strcmp(buff,"STATUS/REJSLOW\r\n") == 0){
		sprintf(response, "%d\r\n", badReject);
		write(fd, response, strlen(response));
	}
	else if(strcmp(buff,"STATUS/REJPROD\r\n") == 0){
		sprintf(response, "%d\r\n", limitProdReject);
		write(fd, response, strlen(response));
	}
	else if(strcmp(buff,"STATUS/REJCONS\r\n") == 0){
		sprintf(response, "%d\r\n", limitConReject);
		write(fd, response, strlen(response));
	}
	close(fd);
}

int
main( int argc, char *argv[] )
{
	struct sockaddr_in	fsin;
	char			buf[BUFSIZE];
	char			*service;
	int				msock;
	int				ssock;
	fd_set			rfds;
	fd_set			afds;
	fd_set			mfds;
	fd_set			mafds;
	int				alen;
	int				fd;
	int				nfds = 0;
	int				mnfds;
	int				rport = 0;
	int				cc;
	struct timeval  tv;
	
	tv.tv_sec = REJECT_TIME;
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
	nfds = msock+1;

	FD_ZERO(&afds);
	FD_SET( msock, &afds );

	for (;;){
		
		memcpy((char *)&rfds, (char *)&afds, sizeof(rfds));
		if (select(nfds, &rfds , (fd_set *)0 , (fd_set *)0, &tv) < 0){
			fprintf( stderr, "server select: %s\n", strerror(errno) );
			exit(-1);
		}

		if (FD_ISSET( msock, &rfds)) {
			int	ssock;
			alen = sizeof(fsin);
			ssock = accept( msock, (struct sockaddr *)&fsin, &alen );
			if (ssock < 0){
				fprintf( stderr, "accept: %s\n", strerror(errno) );
				exit(-1);
			}


			pthread_mutex_lock( &mutex1 );
			if( clientsIndex < MAX_CLIENTS ){
				clientsIndex++;
				pthread_mutex_unlock( &mutex1 );

				FD_SET( ssock, &afds );
				time(&arrivalTime[ssock - 4]);
			
				if ( ssock+1 > nfds )
					nfds = ssock+1;
			} else {
				pthread_mutex_unlock( &mutex1 );
				limitReject++;
				printf("The clients exceeded the maximum.\n\n\n\n\n");
				fflush(stdout);
				close( ssock );
			}
		}


		memcpy((char *)&rfds, (char *)&afds, sizeof(rfds));
		if (select(nfds, &rfds , (fd_set *)0 , (fd_set *)0, &tv) < 0){
			fprintf( stderr, "server select: %s\n", strerror(errno) );
			exit(-1);
		}


		for ( fd = 0; fd < nfds; fd = fd + 1 ){
			if(fd != msock && !FD_ISSET(fd,&rfds) && FD_ISSET(fd, &afds)){
				time_t seconds;
				time(&seconds);
				if(seconds - arrivalTime[fd - 4] > REJECT_TIME){					
					printf("Do not be bad!\nDo not waste my time.\n"); fflush(stdout);
					close(fd);
					badReject++;
					FD_CLR( fd, &afds );
					if ( nfds == fd+1 )
						nfds--;
				}	
			}
			if (fd != msock && FD_ISSET(fd, &rfds)){
				if ( (cc = read( fd, buf, BUFSIZE )) <= 0 ){
					printf( "The client has gone.\n" );
					close(fd);
				}
				else{
			        pthread_t	thr;
                    buf[cc] = '\0';

                    if ( strcmp( buf, PRODUCE ) == 0 ) {
						pthread_mutex_lock( &mutex1 );
						if( producerIndex < MAX_PROD  ) {
							producerIndex++;
							pthread_mutex_unlock( &mutex1 );
							pthread_create(&thr, NULL, produce, (void *)fd);
						}else{
							pthread_mutex_unlock( &mutex1 );
							limitProdReject++;
							limitReject++;
							printf("There are too many producers... closing\n\r");
							fflush(stdout);
							close(fd);
						}
                       
                    }else if( strcmp( buf, CONSUME ) == 0 ){
						pthread_mutex_lock( &mutex1 );
						if( consumerIndex < MAX_CON  ){
							consumerIndex++;
							pthread_mutex_unlock( &mutex1 );
                        	pthread_create(&thr, NULL, consume, (void *)fd);
						}else{
							pthread_mutex_unlock( &mutex1 );
							limitConReject++;
							limitReject++;
							printf("There are too many concumers... closing...\n\r");
							fflush(stdout);
							close(fd);
						}
                    }
					//Code below checks if the command is related to STATUS cmd
					//if yes it works accordingly to the rules if not then the socket will be closed.
					else{
						if(strlen(buf) > 6){
							char* check = substring(buf,0,6);
							if(strcmp(STATUS,check) == 0){
								manageStatus(fd,buf);
							}else{
								printf("Unrecognized command. Closing the socket.");
								fflush(stdout);
								close(fd);
							}
							free(check);
						}else{
							close(fd);
						}
						pthread_mutex_lock(&mutex1);
						clientsIndex--;
						pthread_mutex_unlock(&mutex1);
					}
				}

				FD_CLR( fd, &afds );
				if ( nfds == fd+1 )
					nfds--;
			}

		}
	}
}

