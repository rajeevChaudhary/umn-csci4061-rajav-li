
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"


#define MAX_DISPATCH_THREADS        100
#define MAX_WORKER_THREADS          100
#define MAX_PREFETCH_THREADS        100
#define MAX_CACHE_SIZE              100
#define MAX_REQUEST_QUEUE_LENGTH    100
#define FILENAME_SIZE				1024

enum mode {
	FCFS = 0,
	CRF = 1,
	SFF = 2
};


struct request {
    int fd;
	char filename[FILENAME_SIZE];

	struct request* next;
};

struct cache_entry {
    struct request* req;
    char* data;

    struct cache_entry* next;
};




// START REQUEST QUEUE CODE

struct request* queue_head = NULL;
struct request* queue_tail = NULL;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

void queue_addRequest(int fd, const char* const filename) {
	pthread_mutex_lock(&queue_mutex);

	struct request* newRequest = malloc( sizeof(struct request) );

    newRequest->fd = fd;
	strncpy(newRequest->filename, filename, FILENAME_SIZE);
	newRequest->next = NULL;

	if (queue_head == NULL) { // Queue was empty

		queue_head = queue_tail = newRequest;

	} else { // Queue has at least one element (the head)

		queue_tail->next = newRequest;
		queue_tail = newRequest;

	}

	pthread_mutex_unlock(&queue_mutex);
}

// Returns dynamically allocated buffer with request filename
// Returns NULL on failure -- should never happen with semaphore synchronization!!
// Must be freed!
struct request* queue_fetchRequest() {
	pthread_mutex_lock(&queue_mutex);

    struct request *return_value = queue_head;

	if (queue_head != NULL) // Queue is not empty
		if (queue_head == queue_tail) // Queue has only one element
			queue_head = queue_tail = NULL;
		else
			queue_head = queue_head->next;

	pthread_mutex_unlock(&queue_mutex);

	return return_value;
}

// END REQUEST QUEUE CODE


// SEMAPHORES FOR QUEUE ACCESS
sem_t dispatch_sem; // Starts at value MAX_REQUEST_QUEUE_LENGTH
sem_t worker_sem; // Starts at value 0






// START CACHE CODE

struct cache_entry* cache_head = NULL;
struct cache_entry* cache_tail = NULL;

pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

void cache_addEntry(struct request* req, char* data) {
	pthread_mutex_lock(&queue_mutex);

	struct cache_entry* newEntry = malloc( sizeof(struct cache_entry) );

    newEntry->req = req;
    newEntry->data = data;
    newEntry->next = NULL;

	if (cache_head == NULL) { // Cache was empty

		cache_head = cache_tail = newEntry;

	} else () {


	} else { // Queue has at least one element (the head)

		queue_tail->next = newRequest;
		queue_tail = newRequest;

	}

	pthread_mutex_unlock(&queue_mutex);
}

// Returns dynamically allocated buffer with request filename
// Returns NULL on failure -- should never happen with semaphore synchronization!!
// Must be freed!
struct request* queue_fetchRequest() {
	pthread_mutex_lock(&queue_mutex);

    struct request *return_value = queue_head;

	if (queue_head != NULL) // Queue is not empty
		if (queue_head == queue_tail) // Queue has only one element
			queue_head = queue_tail = NULL;
		else
			queue_head = queue_head->next;

	pthread_mutex_unlock(&queue_mutex);

	return return_value;
}






void dispatch_thread() {

	char filename_buffer[FILENAME_SIZE];

    while() {

        usleep(0);

        int fd = accept_connection();

        if (fd < 0)
            pthread_exit(NULL);

        if ( get_request(fd, filename_buffer) != 0 )
            continue;

        sem_wait(&dispatch_sem);
        queue_addRequest(fd, filename_buffer);
        sem_post(&worker_sem);

    }

}


//TODO: Worker thread code
//FCFS Worker thread
void fcfs_worker_thread() {

    struct request* req;

    while() {

        usleep(0);

        pthread_mutex_lock(&queue_mutex);


        sem_wait(&worker_sem);
        req = queue_fetchRequest();
        sem_post(&dispatch_sem);

        common_worker(req);

        free(req);

    }

}

//CRF Worker thread
void crf_worker_thread() {

    struct request* req;

    while() {

        usleep(0);



        sem_wait(&worker_sem);
        req = queue_fetchRequest();
        sem_post(&dispatch_sem);

        common_worker(req);

        free(req);

    }

}

//SFF Worker thread
void sff_worker_thread() {}




//Common thread code
void common_worker(struct request* req) {}



//TODO: Prefetch thread code
void prefetch_thread() {}







int main(int argc, char *argv[]) {

    if (argc != 9) {
        fprintf(stderr, "Usage: web_server_http <port> <path> <num_dispatch> <num_workers> <num_prefetch> <qlen> <mode> <cache_entries>\n");
        exit(1);
    }

    // argv[1] := <port>
    // Port number on which to accept incoming connections
    init( atoi( argv[1] ) );

    // argv[2] := <path>
    // Web tree root directory
    chdir( argv[2] );

    // argv[3] := <num_dispatch>
    // The number of dispatch threads to start
    int num_dispatch = atoi( argv[3] );
    if (num_dispatch <= 0 || num_dispatch > MAX_DISPATCH_THREADS) {
    	fprintf(stderr, "Error! Invalid number of dispatch threads. (Max is %d)\n", MAX_DISPATCH_THREADS);
    	exit(1);
    }

    // argv[4] := <num_workers>
    // The number of worker threads to start
    int num_workers = atoi( argv[4] );
    if (num_workers <= 0 || num_workers > MAX_WORKER_THREADS) {
    	fprintf(stderr, "Error! Invalid number of worker threads. (Max is %d)\n", MAX_WORKER_THREADS);
    	exit(1);
    }

    // argv[5] := <num_prefetch>
    // The number of prefetch threads to start
    int num_prefetch = atoi( argv[5] );
    if (num_prefetch <= 0 || num_prefetch > MAX_PREFETCH_THREADS) {
    	fprintf(stderr, "Error! Invalid number of prefetch threads. (Max is %d)\n", MAX_PREFETCH_THREADS);
    	exit(1);
    }

    // argv[6] := <qlen>
    // The fixed, bounded length of the request queue
    int qlen = atoi( argv[6] );
    if (qlen <= 0 || qlen > MAX_REQUEST_QUEUE_LENGTH) {
    	fprintf(stderr, "Error! Invalid request queue length. (Max length is %d)\n", MAX_REQUEST_QUEUE_LENGTH);
    	exit(1);
    }

    // argv[7] := <mode>
    // The mode (FCFS, CRF, SFF) to run the worker thread(s) in
    // FCFS: First Come First Serve
    // CRF: Cached Requests First
    // SFF: Smallest File First
    enum mode m = atoi( argv[7] );
    void (*worker_thread) ();

    switch (m) {
    case FCFS:
    	worker_thread = fcfs_worker_thread;
    	break;
    case CRF:
    	worker_thread = crf_worker_thread;
    	break;
    case SFF:
    	worker_thread = sff_worker_thread;
    	break;
    default:
    	fprintf(stderr, "Error! Invalid mode. (Must be 0 for FCFS, 1 for CRF, or 2 for SFF)\n");
    	exit(1);
    }

    // argv[8] := <cache-entries>
    // The size of the cache, in number of entries
    int cache_entries = atoi( argv[8] );
    if (cache_entries <= 0 || cache_entries > MAX_CACHE_SIZE) {
    	fprintf(stderr, "Error! Invalid cache size. (Max size is %d)\n", MAX_CACHE_SIZE);
    	exit(1);
    }



    // Initialize semaphores
    assert( sem_init(&dispatch_sem, 0, MAX_REQUEST_QUEUE_LENGTH) == 0 ); //! Unrecoverable error
    assert( sem_init(&worker_sem, 0, 0) == 0 ); //! Unrecoverable error




    // pthread_join all dispatch threads
    // Set exit flag for worker threads
    // pthread_join all worker threads

    return 0;
}

