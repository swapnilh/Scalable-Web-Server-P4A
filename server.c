#include "cs537.h"
#include "request.h"
#include <pthread.h>
#include <stdlib.h>
#include "mythreads.h"
#include <string.h>
#include <unistd.h>
// 
// server.c: A very, very simple web server
//
// To run:
//  server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//


//Heap Stuff
typedef struct _Data_T
{
    int priority;
    int connfd;
	char requestBuf[MAXLINE];//holds the request line that rio loses once you read it
}Data_T;

typedef struct _Queue_T{
    int size;
    int bufSz;
    Data_T* heapNodes;
}Queue_T;

//Global queue/heap used by producer and consumers
Queue_T q;


//Initialize queue with a dummy node to keep indexing simple
//Alloc buffer number of data nodes
void initQueue(Queue_T* q, int buffers)
{
    q->size = 1;
    q->bufSz = buffers + 1;
    q->heapNodes = (Data_T*) malloc(sizeof(Data_T) * (buffers + 1));
    Data_T dummy = {-1, -1};
    q->heapNodes[0] = dummy;//Index 0 is dummy, root starts at 1
}

int isEmpty(Queue_T* q)
{
    return (q->size == 1); 
}

int isFull(Queue_T* q)
{
    return (q->bufSz == q->size);
}

//Literally the meister of swaps
void swapmeister(Queue_T* q, int ind1, int ind2)
{
	Data_T temp = q->heapNodes[ind1];
	q->heapNodes[ind1] = q->heapNodes[ind2]; 
	q->heapNodes[ind2] = temp;
}

void enqueue(Queue_T* q, Data_T d)
{
    q->heapNodes[q->size] = d;//set new node at the rightmost leaf
	int index=q->size;//start off at that new leaf
    q->size++;
	//percolate up
    while(index != 1)//keep going until root
    {   
		//check parent and child priorities and swap if necessary
        if(q->heapNodes[index].priority < q->heapNodes[index / 2].priority)
			swapmeister(q,index,index/2);   
        else
            break;
        index /= 2;
    }
}




Data_T dequeue(Queue_T* q)
{
    Data_T ret = q->heapNodes[1];
    q->heapNodes[1] = q->heapNodes[(--q->size)];//replace the root by the rightmost leaf and decrement size
    int index = 1;
	//precolate down from root
    while(index*2 < q->size)//while left child exists
    {
		//if right child exists and parent has lower priority than any of the two children is smaller
        if(index*2 + 1 < q->size && 
			(q->heapNodes[index].priority > q->heapNodes[2 * index].priority || q->heapNodes[index].priority > q->heapNodes[2 * index + 1].priority))
        {
            int minIndex;
            if(q->heapNodes[index*2].priority <= q->heapNodes[index* 2 + 1].priority)//left child smaller
                minIndex = index*2;
            else //right child smaller
                minIndex = index* 2 + 1;
            
			swapmeister(q,index,minIndex);
			index = minIndex;
        }
        else if (index*2 + 1 >= q->size && q->heapNodes[index].priority > q->heapNodes[2*index].priority)//parent has lower priority than left child
        {
			swapmeister(q,index,2*index);
            index*=2;
        }
        else //no child exists with priority smaller than parent
            break;
    }
    return ret;//return the original root
}


// Returns file size as priority of the job that made this request.
//404 and other bad forms are send off with the highest priority
void getFileSizeAndRequest(Data_T *job)
{
   struct stat sbuf;
   char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
   char filename[MAXLINE], cgiargs[MAXLINE];
   rio_t rio;

   Rio_readinitb(&rio, job->connfd);
   Rio_readlineb(&rio, buf, MAXLINE);
   sscanf(buf, "%s %s %s", method, uri, version);
   strcpy(job->requestBuf,buf);//copy the buffer with the filename, uri etc
   requestReadhdrs(&rio);

   requestParseURI(uri, filename, cgiargs);

   if (stat(filename, &sbuf) < 0) {
      //requestError(fd, filename, "404", "Not found", "CS537 Server could not find this file");
      job->priority=0;//404 type requests are high priority so they can be quickly handled by the workers 
   }
   else
   {
      job->priority=sbuf.st_size;//file size
   }
}



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

void workerHandle(Data_T* job)
{
	int is_static;
	struct stat sbuf;
   char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
   char filename[MAXLINE], cgiargs[MAXLINE];
   int fd = job->connfd;
   sscanf(job->requestBuf, "%s %s %s", method, uri, version);
   
   if (strcasecmp(method, "GET")) {
      requestError(fd, method, "501", "Not Implemented", "CS537 Server does not implement this method");
      return;
   }
   is_static = requestParseURI(uri, filename, cgiargs);
   if (stat(filename, &sbuf) < 0) {
      requestError(fd, filename, "404", "Not found", "CS537 Server could not find this file");
      return;
   }

   if (is_static) {
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
         requestError(fd, filename, "403", "Forbidden", "CS537 Server could not read this file");
         return;
      }
      requestServeStatic(fd, filename, sbuf.st_size);
   } else {
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
         requestError(fd, filename, "403", "Forbidden", "CS537 Server could not run this CGI program");
         return;
      }
      requestServeDynamic(fd, filename, cgiargs);
   }


}

void *workerStuff(void *arg) {
	int tempconnfd;
	while (1) {
		pthread_mutex_lock(&mutex);
		//printf("Worker locks\n");
		while (isEmpty(&q)) {
		//	printf("Queue empty, worker will wait\n");
			pthread_cond_wait(&work, &mutex);
			
		}
		printf("Worker going to work\n");
		Data_T job=dequeue(&q);//get shortest job from the queue
		tempconnfd = job.connfd;
	//	printf("worker pid=%lu connfd=%d buffer=%p buffer[tail-1]=%d\n", (unsigned long)pthread_self(),tempconnfd, buffer, buffer[tail-1]);
	//	pthread_cond_signal(&work); //Signal other worker threads FIXME is this needed?
		if (!isFull(&q)) {
			pthread_cond_signal(&mast);
			printf("Waking up the master\n");
		}
		pthread_mutex_unlock(&mutex);
		printf("Worker unlocks and going to handle\n");
		//requestHandle(tempconnfd);
		workerHandle(&job);
		Close(tempconnfd);
	}
	return NULL;
}

void *masterStuff(void *arg) {
	int clientlen, tempconnfd;
	struct sockaddr_in clientaddr;
	clientlen = sizeof(clientaddr);
	while (1) {
		tempconnfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
		pthread_mutex_lock(&mutex);
		printf("Master locks\n");
//		printf("confd=%d\n",connfd);
		while(isFull(&q)) { // Queue full !
			printf("Queue full, master will wait\n");
			pthread_cond_wait(&mast, &mutex);
		}
		Data_T job;
		job.connfd = tempconnfd;
		getFileSizeAndRequest(&job);
		enqueue(&q,job);//add the job to the queue
	//	printf("head=%d tail=%d master connfd=%d \n",head, tail, tempconnfd);
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
    initQueue(&q, bufferMax);//initialize queue
	// 
    // CS537: Create some threads...
    //
    pthread_t master;
    pthread_t *worker=malloc(numThreads*sizeof(pthread_t));
    listenfd = Open_listenfd(port);
	// 
	// CS537: In general, don't handle the request in the main thread.
	// Save the relevant info in a buffer and have one of the worker threads 
	// do the work.
	// 
    pthread_create(&master, NULL, masterStuff, NULL);
    int i=0;
    for(; i<numThreads; i++) {
    	pthread_create(&worker[i], NULL, workerStuff, NULL);
    }
    pthread_join(master, NULL);
    return 0;
}


    


 
