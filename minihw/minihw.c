#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>


int m1[1024][1024];
int m2[1024][1024];
int rows;
int cols;
int result[1024][1024];


void *multiply(void *row){
    int r = *((int *) row);
    for(int j = 0; j < rows; j++){
            int mult = 0;
            for(int k = 0; k < cols; k ++){
                mult = mult + m1[r][k] * m2[k][j];
            }
        result[r][j] = mult;
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]){

    if(argc < 5 )
        exit(0);
    
    int         rows = atoi(argv[1]);
    int         cols = atoi(argv[2]);

    if(rows > 1024 || cols > 1024)
        exit(0);

    for(int i = 0; i < rows; i++){
        for(int j = 0; j < cols; j++){
            m1[i][j] = atoi(argv[3]);
        }
    }

    for(int i = 0; i < cols; i++){
        for(int j = 0; j < rows; j++){
            m2[i][j] = atoi(argv[4]);
        }
    }

    int         index[rows];
    pthread_t   threads[rows];
    
    clock_t     begin = clock();

    for(int i = 0; i < rows; i++){
        for(int j = 0; j < rows; j++){
            int mult = 0;
            for(int k = 0; k < cols; k ++){
                mult = mult + m1[i][k] * m2[k][j];
            }
            result[i][j] = mult;
        }
    }

    clock_t     end = clock();
    double      time = (double)(end - begin) * 1000/CLOCKS_PER_SEC;
    
    //printf("Time spent without threads: %f \n", time);
    fprintf(stderr, "Time spent without threads: %f  ms.\n", time);
 
    begin = clock();
    for(int i = 0; i < rows; i++){
        index[i] = i;
        int status = pthread_create(&threads[i], NULL, multiply, (void *) &index[i]);
        if(status != 0)
            exit(-1);
    }

    for (int j = 0; j < rows; j++ )
		pthread_join( threads[j], NULL );
    end = clock();
    
    time = (double)(end - begin)*1000/CLOCKS_PER_SEC;
    
    fprintf(stderr, "Time spent with threads: %f  ms.\n", time);

    for ( int i = 0; i < rows; i++ ){
        for (int j = 0; j < rows; j++ )
            printf( " %d ", result[i][j] );
        printf( "\n" );
    }

     exit(0);
}
