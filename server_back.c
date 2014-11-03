#include "cs537.h"
#include "request.h"
#include <pthread.h>
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
int listenfd, connfd;
pthread_cond_t  work = PTHREAD_COND_INITIALIZER;
pthread_cond_t  mast = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// CS537: Parse the new arguments too
void getargs(int *port, int argc, char *argv[])
{
    if (argc != 2) {
	fprintf(stderr, "Usage: %s <port>\n", argv[0]);
	exit(1);
    }
    *port = atoi(argv[1]);
}

void *workerStuff(void *arg) {
	printf("Worker\n");
	while (1) {
		pthread_mutex_lock(&mutex);
		printf("Worker locks\n");
		pthread_cond_wait(&work, &mutex);
		printf("AFter signal worker\n");
		requestHandle(connfd);
		Close(connfd);
		pthread_cond_signal(&mast);
		pthread_mutex_unlock(&mutex);
		printf("Worker unlocks\n");
	}
	return NULL;
}

void *masterStuff(void *arg) {
	printf("Master\n");
	int clientlen;
	struct sockaddr_in clientaddr;
	clientlen = sizeof(clientaddr);

	while (1) {
		pthread_mutex_lock(&mutex);
		printf("Master locks\n");
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
		printf("Before signal master\n");
		pthread_cond_signal(&work);
		pthread_cond_wait(&mast, &mutex);
		pthread_mutex_unlock(&mutex);
		printf("Master unlocks\n");
	}
	return NULL;
}

int main(int argc, char *argv[])
{
    int port;

    getargs(&port, argc, argv);

    // 
    // CS537: Create some threads...
    //
    pthread_t master, worker;
    char *args=NULL;
    listenfd = Open_listenfd(port);
	// 
	// CS537: In general, don't handle the request in the main thread.
	// Save the relevant info in a buffer and have one of the worker threads 
	// do the work.
	// 
    pthread_create(&master, NULL, masterStuff, &args);
    pthread_create(&worker, NULL, workerStuff, &args);
    pthread_join(master, NULL);
    return 0;
}


    


 
