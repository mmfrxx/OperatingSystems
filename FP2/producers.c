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
#include <math.h>
#include "prodcon.h"


int 		bad;


double poissonRandomInterarrivalDelay( double r ){
    return -(log((double) 1.0 - 
			((double) rand())/((double) RAND_MAX)))/r;
}


int min(int a, int b){
	if(a > b) return b;
	else return a;
}


ITEM *makeItem(){
	int i;
	ITEM *p = malloc( sizeof(ITEM) );
	p->size = random()%MAX_LETTERS;
	return p;
}


char *chunk( int length){
   	char *pointer = malloc(length+1);
   
   	if (pointer == NULL){
    	printf("Unable to allocate memory.\n");
      	exit(1);
   	}
 
	for (int c = 0; c < length; c++ )
		*(pointer+c) = 'X';
	
	*(pointer + length) = '\0';
	return pointer;
}




void *producer(void *servicesock){
    char		buf[BUFSIZE];
	int		    cc;
	char   	    *host = "localhost";
	int 	    csock;
	char	    *service = (char *)servicesock;
	char	    *sending = malloc(BUFSIZE * sizeof(char*));
	int 		idx = 0;

	if ( ( csock = connectsock( host, service, "tcp" )) == 0 )
	{
		fprintf( stderr, "Cannot connect to server.\n" );
		exit( -1 );
	}



	if(rand()%100 < bad){
		usleep(SLOW_CLIENT * 1000000);
		printf("Oops! Bad client! Going to sleep... z Z z Z\n\r");
		fflush(stdout);
	}

	write(csock, "PRODUCE\r\n", 9);

    ITEM *item = makeItem();

    if ( (cc = read( csock, buf, BUFSIZE )) <= 0 ){
        printf( "The server has gone.\n" );
        close(csock);
    } else {

        buf[cc] = '\0';
        if(strcmp(buf,"GO\r\n") == 0){
			int reversedSize = htonl(item->size);
            write( csock, &reversedSize, 4 );

			if ( (cc = read( csock, buf, BUFSIZE )) <= 0 ){
				printf( "The server has gone.\n" );
				close(csock);
			} else {
				buf[cc] = '\0';
        		if(strcmp(buf,"GO\r\n") == 0){
					while(idx < item->size){
						char *substr = chunk(min(BUFSIZE, item->size - idx ));
						write( csock, substr, min(BUFSIZE, item->size - idx ));
						idx = idx + BUFSIZE;
						free(substr);
					}
				}else{
					close(csock);
				}
			}
        } 
		cc = read( csock, buf, BUFSIZE );
		close(csock);
	}
	free(item);
	pthread_exit(NULL);
}


int
main( int argc, char *argv[] )
{
	char			*service;
	int         	     num;
	int 			    i, j;

	int	    		   csock;
	char *host = "localhost";
	double				rate;
	
	switch( argc ){
		case 6:
			strcpy(host,argv[1]);
			service = argv[2];
			num = atoi(argv[3]);
			rate = atof(argv[4]);
			bad = atoi(argv[5]);

		case    5:
			service = argv[1];
			num = atoi(argv[2]);
			rate = atof(argv[3]);
			bad = atoi(argv[4]);
			break;
		default:
			fprintf( stderr, "the wrong number of parameters\n" );
			exit(-1);
	}

	if(num > 2000){
		printf("Sorry, the number you provided is too large.\nPlease,try again.");
		fflush(stdout);
		exit(-1);
	}else if(rate <= 0.0){
		printf("Sorry, rate must be greater than 0.0. Try again.");
		fflush(stdout);
		exit(-1);
	}else if(bad > 100 || bad < 0 ){
		printf("Sorry, bad you provided is incorrect.\nPlease,try again.");
		fflush(stdout);
		exit(-1);
	}

	pthread_t threads[num];

	for( i = 0; i < num; i++){
		int status = pthread_create(&threads[i], NULL, producer, (void *) service);
		if(status != 0){
			printf("Error\n");
			exit(-1);
		}

		double waiting_time = poissonRandomInterarrivalDelay(rate); 
		usleep(waiting_time * 1000000);
		printf("Now we will wait for %lf seconds\n\r", waiting_time);
		fflush(stdout);

	}

	for ( j = 0; j < num; j++ )
		pthread_join( threads[j], NULL );

	pthread_exit(0);

}
