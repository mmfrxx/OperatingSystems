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


void *consumer(void *servicesock){
    int 	 numberOfChars;
	char     *host = "localhost";
	int 	 csock;
	char	 *service = (char *)servicesock;
	int 	 cc;
	int 	 index = 0;
	int 	 receivedBytes = 0;
	int  	 devNull = open("/dev/null", O_WRONLY );
	int 	 id = pthread_self();
	char 	 filename[45];
	char	 *arrived = malloc(BUFSIZE * sizeof(char*));
	
	sprintf(filename, "./text/%ld.txt", id);
	int fd = open(filename,O_RDWR|O_CREAT, S_IRWXU);

	if ( ( csock = connectsock( host, service, "tcp" )) == 0 ){
		fprintf( stderr, "Cannot connect to server in consumer.\n" );
		write(fd, REJECT, strlen(REJECT));
	}
	else{
		if(rand()%100 < bad){
			printf("Oops! Bad client! Going to sleep... z Z z Z\n\r");
			fflush(stdout);
			usleep(SLOW_CLIENT * 1000000);
		}

		write(csock, "CONSUME\r\n",9);
		fflush( stdout );

		if ((cc =  read(csock, &numberOfChars, 4)) <= 0 ){
			printf( "The server has gone.\n" );
			write(fd, REJECT, strlen(REJECT));
			close(csock);
		}else{
			numberOfChars = htonl(numberOfChars);

			while (( cc = read(csock, arrived, min(BUFSIZE, numberOfChars - receivedBytes))) > 0 ){
				receivedBytes = receivedBytes + cc;
				write(devNull,arrived,cc);
				if(receivedBytes >= numberOfChars) break;
			}

			char bytes[45];
			sprintf(bytes, " %d", receivedBytes);
			if(receivedBytes == numberOfChars){
				write(fd, SUCCESS,strlen(SUCCESS));
				write(fd, bytes, strlen(bytes));
			}else if(receivedBytes != numberOfChars){
				write(fd,BYTE_ERROR, strlen(BYTE_ERROR));
				write(fd, bytes, strlen(bytes));
			}
		}
	}

	close(csock);
	pthread_exit(NULL);
}

int main( int argc, char *argv[] )
{
	char		buf[BUFSIZE];
	char		*service;		
	char		*host = "localhost";
	int			cc;
	int			csock;
	int         num;
	double		rate;

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

	for( int i = 0; i < num; i++){
		int status = pthread_create(&threads[i], NULL, consumer, (void *) service);
		if(status != 0){
			printf("Error\n");
			exit(-1);
		}
		
		double waiting_time = poissonRandomInterarrivalDelay(rate); 
		usleep(waiting_time * 1000000);
		printf("Now we will wait for %lf seconds\r\n", waiting_time);
		fflush(stdout);

	}

	for ( int j = 0; j < num; j++ )
		pthread_join( threads[j], NULL );

	pthread_exit(0);
}
