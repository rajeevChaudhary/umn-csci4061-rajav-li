
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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








// MurmurHash 2 algorithm created by Austin Appleby. Code is in the public domain.
// (Current version is MurmurHash 3; This version is simpler.)
uint32_t MurmurHash2 ( const void * key, int len, uint32_t seed )
{
  // 'm' and 'r' are mixing constants generated offline.
  // They're not really 'magic', they just happen to work well.

  const uint32_t m = 0x5bd1e995;
  const int r = 24;

  // Initialize the hash to a 'random' value

  uint32_t h = seed ^ len;

  // Mix 4 bytes at a time into the hash

  const unsigned char * data = (const unsigned char *)key;

  while(len >= 4)
  {
    uint32_t k = *(uint32_t*)data;

    k *= m;
    k ^= k >> r;
    k *= m;

    h *= m;
    h ^= k;

    data += 4;
    len -= 4;
  }

  // Handle the last few bytes of the input array

  switch(len)
  {
  case 3: h ^= data[2] << 16;
  case 2: h ^= data[1] << 8;
  case 1: h ^= data[0];
      h *= m;
  };

  // Do a few final mixes of the hash to ensure the last few
  // bytes are well-incorporated.

  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;

  return h;
}












struct request {
    int fd;
	char filename[FILENAME_SIZE];
	intmax_t filesize;

	struct request* next;
    struct request* prev;
};

struct cache_entry {
    char filename[FILENAME_SIZE];
    char* filedata;
    intmax_t filesize;

    struct cache_entry* next;
    struct cache_entry* prev;

    struct cache_entry* next_collision;
    struct cache_entry* prev_collision;
};

struct cached_request {
	struct request* req;
	char* filedata;
};

struct request *queue_sentinel;
unsigned queue_size;

struct request *prefetch_sentinel;
unsigned prefetch_size;

struct cache_entry *cache_sentinel;
struct cache_entry *hash_table; // Static array of size max_cache_size
unsigned cache_size;
unsigned max_cache_size; // Set by user, will not exceed MAX_CACHE_SIZE













// LIST-ADD, LIST-REMOVE, and LIST-SHIFT defined on the queue
struct request* q_add(int fd, const char* const filename, intmax_t filesize) {

	struct request* r = malloc(sizeof(struct request));

	r->fd = fd;
	strncpy(r->filename, filename, FILENAME_SIZE);
	r->filesize = filesize;

	queue_sentinel->prev->next = r;
	r->prev = queue_sentinel->prev;
	queue_sentinel->prev = r;
	r->next = queue_sentinel;

	++queue_size;

	return r;
}

void q_remove(struct request* r) {

	if (r != queue_sentinel) {
		r->prev->next = r->next;
		r->next->prev = r->prev;

		--queue_size;
	}

}

struct request* q_shift() {

	struct request* head = queue_sentinel->next;

	if (head != queue_sentinel) {
		q_remove(head);
		return head;
	}
	else
		return NULL;
}




// LIST-ADD, LIST-REMOVE, and LIST-SHIFT defined on the prefetch queue
struct request* p_add(struct request* r) {

	prefetch_sentinel->prev->next = r;
	r->prev = prefetch_sentinel->prev;
	prefetch_sentinel->prev = r;
	r->next = prefetch_sentinel;

	++prefetch_size;

	return r;
}

void p_remove(struct request* r) {

	if (r != prefetch_sentinel) {
		r->prev->next = r->next;
		r->next->prev = r->prev;

		--prefetch_size;
	}

}

struct request* p_shift() {

	struct request* head = prefetch_sentinel->next;

	if (head != prefetch_sentinel) {
		q_remove(head);
		return head;
	}
	else
		return NULL;
}






//HASHED-LIST functions for the cache
void hash_add(struct cache_entry* e) {
	e->next_collision = NULL;
	e->prev_collision = NULL;

	unsigned hash = MurmurHash2(e->filename, 1024, 436) % 100;

	if (hash_table[hash] == NULL) {
		hash_table[hash] = e;
	} else {
		struct cache_entry* current = hash_table[hash];
		while (current->next_collision != NULL)
			current = current->next_collision;
		current->next_collision = e;
		e->prev_collision = current;
	}
}

struct cache_entry* c_add(const char* const filename, const char* const filedata, intmax_t filesize) {

	struct cache_entry* e = malloc(sizeof(struct cache_entry));

	strncpy(e->filename, filename, 1024);
	e->filedata = filedata;
	e->filesize = filesize;

	cache_sentinel->prev->next = e;
	e->prev = cache_sentinel->prev;
	cache_sentinel->prev = e;
	e->next = cache_sentinel;

	++cache_size;

	hash_add(e);

	return e;
}

void hash_remove(struct cache_entry* e) {

	if (e->prev_collision == NULL) {
		unsigned hash = MurmurHash2(e->filename, 1024, 436) % 100;

		if (e->next_collision == NULL) // This hash is now empty
			hash_table[hash] = NULL;
		else
			hash_table[hash] = e->next_collision;
	} else {
		e->prev_collision->next_collision = e->next_collision;
		if (e->next_collision != NULL)
			e->next_collision->prev_collision = e->prev_collision;
	}

}

void c_remove(struct cache_entry* e) {

	if (e != cache_sentinel) {
		e->prev->next = e->next;
		e->next->prev = e->prev;

		--cache_size;
	}

	hash_remove(e);

}

struct cache_entry* c_shift() {

	struct cache_entry* head = cache_sentinel->next;

	if (head != cache_sentinel) {
		list_remove(head);
		return head;
	}
	else
		return NULL;
}












// START CACHE CODE

struct cache_entry* cache_head = NULL;
struct cache_entry* cache_tail = NULL;
unsigned current_cache_size = 0;

pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

void cache_addEntry(const char* const filename, char* data) {

	struct cache_entry* newEntry = malloc( sizeof(struct cache_entry) );

    strncpy(newEntry->filename, filename, FILENAME_SIZE);
    newEntry->data = data;
    newEntry->next = NULL;

	pthread_mutex_lock(&cache_mutex);

	if (cache_head == NULL) { // Cache is empty

		cache_head = cache_tail = newEntry;

	} else if (current_cache_size == max_cache_size) { // Cache is full

        //Replace the oldest entry in the cache with the new entry

        struct cache_entry* old_head = cache_head;
        cache_head = cache_head->next;
        free(old_head);

        if (max_cache_size == 1) { // In this case, cache_tail == cache_head, which we just freed
            cache_head = cache_tail = newEntry;
        } else { // current_cache_size (and max_cache_size) are > 1, don't worry about cache_head
            cache_tail->next = newEntry;
            cache_tail = newEntry;
        }


	} else { // Cache has at least one element (the head) and is not full

		cache_tail->next = newEntry;
		cache_tail = newEntry;

	}

    ++current_cache_size;

	pthread_mutex_unlock(&cache_mutex);
}

// Returns the cache entry that matches the given filename or NULL if no entry matched
//! USAGE OF THE CACHE ENTRY MUST BE SYNCHRONIZED OVER cache_mutex
struct cache_entry* cache_search(const char* const filename) {

    struct cache_entry* entry = cache_head;

    while (entry != NULL) {
        if (strncmp(entry->filename, filename, FILENAME_SIZE) == 0)
            break;
        entry = entry->next;
    }

    return entry;
}

// END CACHE CODE




// START REQUEST QUEUE CODE

struct request* queue_head = NULL;
struct request* queue_tail = NULL;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

void queue_addRequest(int fd, const char* const filename) {

	struct request* newRequest = malloc( sizeof(struct request) );

    newRequest->fd = fd;
	strncpy(newRequest->filename, filename, FILENAME_SIZE);
	newRequest->next = NULL;

    pthread_mutex_lock(&queue_mutex);

	if (queue_head == NULL) { // Queue is empty

		queue_head = queue_tail = newRequest;

	} else { // Queue has at least one element (the head)

		queue_tail->next = newRequest;
		queue_tail = newRequest;

	}

	pthread_mutex_unlock(&queue_mutex);

}

// Returns the request at the head of the queue and removes it from the queue
// Returns NULL on failure -- should never happen with semaphore synchronization!!
// The request must be freed by the caller!
//
// Calls to this function don't need to be synchronized externally because the request is removed from the queue immediately
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


// Returns the first request in the queue that matches a cache entry, and removes it from the queue
// Returns NULL if no such request can be found
// The request must be freed by the caller!
int fetchFirstCachedRequest(struct request** return_req, struct cache_entry** return_entry) {
    pthread_mutex_lock(&queue_mutex);
    pthread_mutex_lock(&cache_mutex);

    struct request* prev = NULL;
    struct request* req = queue_head;
    struct cache_entry* entry = cache_head;

    if (entry != NULL) {
        while (req != NULL) {
            while (entry != NULL) {
                if (strncmp(req->filename, entry->filename, FILENAME_SIZE) == 0)
                    break;
                entry = entry->next;
            }

            if (entry != NULL)
                break;

            prev = req;
            req = req->next;
        }
    }

    if (entry != NULL) {
        if (prev == NULL) { // The head of the queue is the first cached request

        }

        prev->next = req->next;

    }

    pthread_mutex_unlock(&queue_mutex);
    pthread_mutex_unlock(&cache_mutex);

    return 1;
}



// SEMAPHORES FOR QUEUE ACCESS
sem_t dispatch_sem; // Starts at maximum queue length set by user, will not exceed MAX_REQUEST_QUEUE_LENGTH
sem_t worker_sem; // Starts at value 0













void dispatch_thread() {

	char filename_buffer[FILENAME_SIZE];

    for (;;) {

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

    for (;;) {

        usleep(0);

        pthread_mutex_lock(&queue_mutex);


        sem_wait(&worker_sem);
        req = queue_fetchRequest();
        sem_post(&dispatch_sem);

        //common_worker(req);

        free(req);

    }

}

//CRF Worker thread
void crf_worker_thread() {

    struct request* req;

    for (;;) {

        usleep(0);



        sem_wait(&worker_sem);
        req = queue_fetchRequest();
        sem_post(&dispatch_sem);

        //common_worker(req);

        free(req);

    }

}

//SFF Worker thread
void sff_worker_thread() {}




//Common thread code
//void common_worker(struct request* req) {}



//TODO: Prefetch thread code
void prefetch_thread() {}







int main(int argc, char *argv[]) {

    if (argc != 9) {
        fprintf(stderr, "Usage: web_server_http <port> <path> <num_dispatch> <num_workers> <num_prefetch> <qlen> <mode> <cache_entries>\n");
        exit(1);
    }




    queue_sentinel = malloc( sizeof(struct request) );
    queue_sentinel->next = &queue_sentinel;
    queue_sentinel->prev = &queue_sentinel;

    prefetch_sentinel = malloc( sizeof(struct request) );
    prefetch_sentinel->next = &prefetch_sentinel;
    prefetch_sentinel->prev = &prefetch_sentinel;




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
    max_cache_size = atoi( argv[8] );
    if (max_cache_size <= 0 || max_cache_size > MAX_CACHE_SIZE) {
    	fprintf(stderr, "Error! Invalid cache size. (Max size is %d)\n", MAX_CACHE_SIZE);
    	exit(1);
    }



    // Initialize semaphores
    assert( sem_init(&dispatch_sem, 0, qlen) == 0 ); //! Unrecoverable error
    assert( sem_init(&worker_sem, 0, 0) == 0 ); //! Unrecoverable error




    // pthread_join all dispatch threads
    // Set exit flag for worker threads
    // pthread_join all worker threads

    return 0;
}

