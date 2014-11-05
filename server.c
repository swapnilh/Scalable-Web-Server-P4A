#include "cs537.h"
#include "request.h"
#include <pthread.h>
#include <stdlib.h>
#include "mythreads.h"
// 
// server.c: A very, very simple web server
//
// To run:
//  server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//
int listenfd, connfd, bufferMax, head, tail;
pthread_cond_t  work = PTHREAD_COND_INITIALIZER;
pthread_cond_t  mast = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// CS537: Parse the new arguments too
void getargs(int *port, int *numThreads, int *bufferMax, int argc, char *argv[])
{
    if (argc != 4) {
	fprintf(stderr, "Usage: %s <portnum> <threads> <buffers>\n", argv[0]);
	exit(1);
    }
    if (atoi(argv[2]) <= 0) {
	fprintf(stderr, "Number of threads should be greater than 0!\n");
	exit(1);
    }
    if (atoi(argv[3]) <= 0) {
	fprintf(stderr, "Buffer size should be greater than 0!\n");
	exit(1);
    }
    *port = atoi(argv[1]);
    *numThreads = atoi(argv[2]);
    *bufferMax = atoi(argv[3]);
}

void *workerStuff(void *arg) {
	int *buffer = (int *)(arg);
	int tempconnfd;
	while (1) {
		pthread_mutex_lock(&mutex);
		printf("Worker locks\n");
		while (head == tail) {
			printf("Queue empty, worker will wait\n");
			pthread_cond_wait(&work, &mutex);
		}
		tempconnfd = buffer[tail];
		tail=(tail==bufferMax)?0:tail+1;
		printf("worker pid=%lu connfd=%d buffer=%p buffer[tail-1]=%d\n", (unsigned long)pthread_self(),tempconnfd, buffer, buffer[tail-1]);
		pthread_cond_signal(&work); //Signal other worker threads FIXME is this needed?
		if (head+2%bufferMax == tail) {
			pthread_cond_signal(&mast);
			printf("Waking up the master\n");
		}
		pthread_mutex_unlock(&mutex);
		printf("Worker unlocks\n");
		requestHandle(tempconnfd);
		Close(tempconnfd);
	}
	return NULL;
}

void *masterStuff(void *arg) {
	int clientlen, tempconnfd;
	struct sockaddr_in clientaddr;
	clientlen = sizeof(clientaddr);
	int *buffer = (int *)(arg);
	int next_head=0;
	while (1) {
		printf("before accept!\n");
		tempconnfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
		printf("after accept!\n");
		pthread_mutex_lock(&mutex);
		printf("Master locks\n");
//		printf("confd=%d\n",connfd);
		next_head=(head==bufferMax)?0:head+1;
		while(next_head == tail) { // Queue full !
			printf("Queue full, master will wait\n");
			pthread_cond_wait(&mast, &mutex);
		}
		buffer[head] = tempconnfd;
		head = next_head;
		printf("head=%d tail=%d master connfd=%d \n",head, tail, tempconnfd);
		pthread_cond_signal(&work);
		pthread_mutex_unlock(&mutex);
		printf("Master unlocks\n");
	}
	return NULL;
}

int main(int argc, char *argv[])
{
    int port, numThreads;

    getargs(&port, &numThreads, &bufferMax, argc, argv);
    int *buffer = malloc(bufferMax*sizeof(int));; 
    // 
    // CS537: Create some threads...
    //
    pthread_t master;
    pthread_t *worker=malloc(numThreads*sizeof(pthread_t));
    char *args[1];
    args[0] = (char*)&buffer;
    listenfd = Open_listenfd(port);
	// 
	// CS537: In general, don't handle the request in the main thread.
	// Save the relevant info in a buffer and have one of the worker threads 
	// do the work.
	// 
    pthread_create(&master, NULL, masterStuff, &args);
    int i=0;
    for(; i<numThreads; i++) {
    	pthread_create(&worker[i], NULL, workerStuff, &args);
    }
    pthread_join(master, NULL);
    return 0;
}


    


 
