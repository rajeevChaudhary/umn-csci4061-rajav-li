
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util.h"


#define MAX_DISPATCH_THREADS        100
#define MAX_WORKER_THREADS          100
#define MAX_PREFETCH_THREADS        100
#define MAX_CACHE_SIZE              100
#define MAX_REQUEST_QUEUE_LENGTH    100
#define MAX_PREFETCH_QUEUE_LENGTH	100
#define FILENAME_SIZE				1024
#define HASH_SEED					42



// Application meta information
enum mode {
	FCFS = 0,
	CRF = 1,
	SFF = 2
};

FILE *logfile; //File descriptor ("web_server_log" in web tree root dir)
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

int global_exit = 0;




int guess_next(char *filename, char *guessed);






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

struct request_bundle {
	struct request* req;
	struct cache_entry* ent;
};


struct request *queue_sentinel;
int queue_size;
int max_queue_size;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_get_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_put_cond = PTHREAD_COND_INITIALIZER;

struct request *prefetch_sentinel;
int prefetch_size;
int max_prefetch_size = MAX_PREFETCH_QUEUE_LENGTH;
pthread_mutex_t prefetch_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t prefetch_get_cond = PTHREAD_COND_INITIALIZER;

struct cache_entry *cache_sentinel;
struct cache_entry *cache_hashtable; // Static array of size max_cache_size
int cache_size;
int max_cache_size; // Set by user, will not exceed MAX_CACHE_SIZE
pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

// This function expects max_cache_size to be initialized
void init_lists() {
	queue_sentinel = malloc( sizeof(struct request) );
	queue_sentinel->next = queue_sentinel;
	queue_sentinel->prev = queue_sentinel;

	prefetch_sentinel = malloc( sizeof(struct request) );
	prefetch_sentinel->next = prefetch_sentinel;
	prefetch_sentinel->prev = prefetch_sentinel;

	cache_sentinel = malloc( sizeof(struct cache_entry) );
	cache_sentinel->next = cache_sentinel;
	cache_sentinel->prev = cache_sentinel;
	cache_sentinel->next_collision = NULL;
	cache_sentinel->prev_collision = NULL;
	cache_hashtable = malloc( sizeof(struct cache_entry) * max_cache_size );
}



struct request* createRequest(int fd, const char* const fn, intmax_t fs) {
	struct request* req = malloc( sizeof(struct request) );

	req->fd = fd;
	strncpy(req->filename, fn, FILENAME_SIZE);
	req->filesize = fs;

	req->next = NULL;
	req->prev = NULL;

	return req;
}

void destroyRequest(struct request* req) {
	free( req );
}




struct cache_entry* createCacheEntry(const char* const fn, char* data, intmax_t fs) {
	struct cache_entry* ent = malloc( sizeof(struct cache_entry) );

	strncpy(ent->filename, fn, FILENAME_SIZE);
	ent->filedata = data;
	ent->filesize = fs;

	ent->next = NULL;
	ent->prev = NULL;
	ent->next_collision = NULL;
	ent->prev_collision = NULL;

	return ent;
}

// should destroy file data? yes.
void destroyCacheEntry(struct cache_entry* ent) {
	free( ent->filedata );
	free( ent );
}








// LIST-ADD, LIST-REMOVE, and LIST-SHIFT defined on the queue
void q_add(struct request* r) {

	queue_sentinel->prev->next = r;
	r->prev = queue_sentinel->prev;
	queue_sentinel->prev = r;
	r->next = queue_sentinel;

	++queue_size;

}

void q_remove(struct request* r) {

	if (r != queue_sentinel) {
		r->prev->next = r->next;
		r->next->prev = r->prev;

		--queue_size;
	}

}

struct request* q_first() {

	struct request* head = queue_sentinel->next;

	if (head != queue_sentinel)
		return head;
	else
		return NULL;

}

struct request* q_nextOf(struct request* r) {

	r = r->next;

	if (r == queue_sentinel)
		return NULL;
	else
		return r;

}

struct request* q_shift() {

	struct request* first = q_first();

	if (first != NULL)
		q_remove(first);

	return first;

}





// LIST-ADD, LIST-REMOVE, and LIST-SHIFT defined on the prefetch queue
void p_add(struct request* r) {

	prefetch_sentinel->prev->next = r;
	r->prev = prefetch_sentinel->prev;
	prefetch_sentinel->prev = r;
	r->next = prefetch_sentinel;

	++prefetch_size;

}

void p_remove(struct request* r) {

	if (r != prefetch_sentinel) {
		r->prev->next = r->next;
		r->next->prev = r->prev;

		--prefetch_size;
	}

}

struct request* p_first() {

	struct request* head = prefetch_sentinel->next;

	if (head == prefetch_sentinel)
		return NULL;
	else
		return head;

}

struct request* p_nextOf(struct request* r) {

	r = r->next;

	if (r == prefetch_sentinel)
		return NULL;
	else
		return r;

}

struct request* p_shift() {

	struct request* first = p_first();

	if (first != NULL)
		p_remove(first);

	return first;

}






//HASHED-LIST functions for the cache
void hash_add(struct cache_entry* e) {
	e->next_collision = NULL;
	e->prev_collision = NULL;

	int hash = MurmurHash2(e->filename, FILENAME_SIZE, HASH_SEED) % max_cache_size;

	if (cache_hashtable[hash] == NULL) {
		cache_hashtable[hash] = e;
	} else {
		struct cache_entry* current = cache_hashtable[hash];
		while (current->next_collision != NULL)
			current = current->next_collision;
		current->next_collision = e;
		e->prev_collision = current;
	}
}

void hash_remove(struct cache_entry* e) {

	if (e->prev_collision == NULL) {
		int hash = MurmurHash2(e->filename, FILENAME_SIZE, HASH_SEED) % max_cache_size;

		if (e->next_collision == NULL) // This hash is now empty
			cache_hashtable[hash] = NULL;
		else
			cache_hashtable[hash] = e->next_collision;
	} else {
		e->prev_collision->next_collision = e->next_collision;
		if (e->next_collision != NULL)
			e->next_collision->prev_collision = e->prev_collision;
	}

}

struct cache_entry* hash_search(const char* const fn) {

	int hash = MurmurHash2(fn, FILENAME_SIZE, HASH_SEED) % max_cache_size;

	return cache_hashtable[hash];

}

void c_add(struct cache_entry* e) {

	cache_sentinel->prev->next = e;
	e->prev = cache_sentinel->prev;
	cache_sentinel->prev = e;
	e->next = cache_sentinel;

	++cache_size;

	hash_add(e);

}

void c_remove(struct cache_entry* e) {

	if (e != cache_sentinel) {
		e->prev->next = e->next;
		e->next->prev = e->prev;

		--cache_size;
	}

	hash_remove(e);

}

struct cache_entry* c_first() {

	struct cache_entry* head = cache_sentinel->next;

	if (head == cache_sentinel)
		return NULL;
	else
		return head;

}

// No nextOf() operation, we have no need to iterate over the cache

struct cache_entry* c_shift() {

	struct cache_entry* first = c_first();

	if (first != NULL)
		c_remove(first);

	return first;

}


// Must be locked over the cache - can be unlocked immediately following the call
// No other synchronization needed, since old entries will automatically be displaced once cache is full
void cache_putEntry(struct cache_entry* ent) {

	if (cache_size >= max_cache_size) // Should never be > (We should check! That would be a leak!)
		destroyCacheEntry( c_shift() ); // This will also free the file data stored in the entry

	c_add(ent);

}

// Returns a pointer to the first cache entry matching the given filename
// Returns NULL if no such entry exists
// Must be locked over the cache INCLUDING ALL USAGE OF THE RETURNED CACHE ENTRY
// The cache entry REMAINS IN THE CACHE
// The returned cache entry should NOT be destroyed upon falling out of the caller's context
struct cache_entry* cache_search(const char* const fn) {

	return hash_search(fn);

}





// Must be locked over the queue - can be unlocked immediately following the call
// Must be synchronized over the "queue put" semaphore
// The "queue get" semaphore should be incremented following the call
void queue_putRequest(struct request* req) {

	q_add(req);

}

// Must be locked over the queue - can be unlocked immediately following the call
// Must be synchronized over the "queue get" semaphore
// The "queue put" semaphore should be incremented following the call
// The returned request is freed from the list and must be destroyed before it goes out of context
struct request_bundle queue_getRequest() {

	struct request* req = q_shift();
	struct cache_entry* ent;
	if (req == NULL)
		ent = NULL;
	else
		ent = cache_search(req->filename);

	struct request_bundle bundle = { req, ent };
	return bundle;

}

// Must be locked over the queue - can be unlocked immediate following the call (safe to use returned request)
// Must be synchronized over the "queue get" semaphore
// The "queue put" semaphore should be incremened following the call
// The returned request is freed from the list and must be destroyed before it goes out of context
struct request_bundle queue_getSmallRequest() {

	struct request* req = q_first();
	if (req == NULL) {
		struct request_bundle bundle = { NULL, NULL };
		return bundle;
	}

	//assert (queue_size != 0);

	struct request* small = req;
	intmax_t smallsize = req->filesize;

	while ( (req = q_nextOf(req)) != NULL ) {
		if (req->filesize < smallsize) {
			small = req;
			smallsize = req->filesize;
		}
	}

	struct cache_entry* ent = cache_search(small->filename);

	struct request_bundle bundle = { small, ent };

	return bundle;

}



// Must be locked over the prefetch queue - can be unlocked immediately following the call
// Should not be called if the prefetch queue is already full (check that prefetch_size < max_prefetch_size)
// The "prefetch get" semaphore should be incremented following this call
void prefetch_putRequest(struct request* req) {

	p_add(req);

}

// Must be locked over the prefetch queue - can be unlocked immediately following the call
// The returned request is freed from the list and must be destroyed before it goes out of context
// Must be synchronized over the "prefetch get" semaphore
struct request* prefetch_getRequest() {

	return p_shift();

}






// Returns request_bundle containing the first request/cache_entry pair if such a pair exists
// Otherwise, returns a request_bundle containing only the first request in the queue
// request_bundle is returned by value
// Must be locked over the request queue - can be unlocked immediately following the call
//    The returned request is removed from the queue
// Must be locked over the cache INCLUDING ALL USAGE OF THE RETURNED CACHE ENTRY
//    The cache entry REMAINS IN THE CACHE
// The returned request is freed from the queue and must be destroyed upon falling out of context
// The returned cache entry should NOT be destroyed upon falling out of the caller's context
struct request_bundle getCachedRequest() {

	struct request* req = q_first();
	struct cache_entry* ent = NULL;

	while (req != NULL) {
		ent = cache_search(req->filename);
		if (ent != NULL) {
			q_remove(req);
			break;
		}
		req = q_nextOf(req);
	}

	if (req == NULL)
		req = queue_getRequest();

	struct request_bundle bundle = { req, ent };

	return bundle;

}






// Reads in the given file and returns a pointer to the data in memory or NULL on failure
char * getFile(const char* const filename) {
	FILE* file = fopen(filename, "rb");
	intmax_t fileLength;

	if (file == NULL)
		return NULL; //Error

	fseek(file, 0, SEEK_END);
	fileLength = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* data = (char *)malloc( fileLength + 1 );
	if (data == NULL) {
		fclose(file);
		return NULL; //Error allocating
	}

	assert( fread(data, fileLength, 1, file) == 1 );
	fclose(file);

	return data;
}

intmax_t getFileSize(const char* const filename) {
	static struct stat stat_buf;

	if ( stat(filename, &stat_buf) == 0 )
		return (intmax_t)stat_buf.st_size;
	else
		return -1;
}

const char* getFileContentType(const char* const filename) {
	const char* extension = strrchr(filename, '.');
	if (extension == NULL)
		return "text/plain";

	++extension;

	if (strcmp(extension, "htm") == 0 || strcmp(extension, "html") == 0)
		return "text/html";
	else if (strcmp(extension, "gif") == 0)
		return "image/gif";
	else if (strcmp(extension, "jpg") == 0 || strcmp(extension, "jpeg") == 0)
		return "image/jpeg";
	else
		return "text/plain";
}

// Must be synchronized over the log mutex
void logRequest(struct request_bundle bundle, int thread_id, int requests_handled, const char* error) {
	fprintf(logfile,"[%d][%d][%d][%s][%s]",
			thread_id,
			requests_handled,
			bundle.req->fd,
			bundle.ent == NULL ? "MISS" : "HIT",
			bundle.req->filename);
	if (error == NULL)
		fprintf(logfile, "[%jd]\n", bundle.req->filesize);
	else
		fprintf(logfile, "[%s]\n", error);
}

const char* process_request(struct request_bundle bundle) {

	assert( bundle.req != NULL );

	char* filename = bundle.req->filename;
	char* data;
	intmax_t filesize;

	const char* error = NULL;

	if (bundle.ent != NULL) {
		data = bundle.ent->filedata;
		filesize = bundle.ent->filesize;
	} else {
		data = getFile(filename);

		filesize = getFileSize(filename);

		if (filesize == -1)
			error = "Error getting file size";

		cache_putEntry(  createCacheEntry( filename, data, filesize )  );
		bundle.req->filesize = filesize;
	}

	if (error == NULL)
		return_result( bundle.req->fd, getFileContentType(filename), data, (int)filesize );
	else
		return_error( bundle.req->fd, error );

	return error;

}


void *dispatch_thread(void *ignored) {
	int fd;
	char filename[1024];
	intmax_t filesize;
	struct request* req;

	while ( (fd = accept_connection()) >= 0 ) {
		if (get_request(fd, &filename) == 0) {
			if ((filesize = getFilesize(filename)) == -1)
				fprintf(stderr, "Error getting file size in dispatch thread\n");
			else {
				req = createRequest(fd, filename, filesize);

				pthread_mutex_lock(&queue_mutex);

				assert(queue_size <= max_queue_size);

				if (queue_size == max_queue_size)
					pthread_cond_wait(&queue_put_cond);

				queue_putRequest(req);

				pthread_cond_signal(&queue_get_cond);

				pthread_mutex_unlock(&queue_mutex);
			}
		}
	}

	pthread_exit();
}

struct request_bundle (*getRequest)();

void *worker_thread(void *ignored) {
	struct request_bundle bundle;
	int thread_id = pthread_self();
	int requests_handled = 0;
	const char* error;

	while (global_exit == 0) {
		pthread_mutex_lock(&queue_mutex);

		assert (queue_size >= 0);

		if (queue_size == 0)
			pthread_cond_wait(&queue_get_cond, &queue_mutex);

		pthread_mutex_lock(&cache_mutex);

		bundle = (*getRequest)();

		pthread_cond_signal(&queue_put_cond);
		pthread_mutex_unlock(&queue_mutex);

		error = processRequest(bundle);

		++requests_handled;

		pthread_mutex_lock(&log_mutex);
		logRequest(bundle, thread_id, requests_handled, error);
		pthread_mutex_unlock(&log_mutex);

		pthread_mutex_unlock(&cache_mutex); // bundle.ent should NOT be used after this point

		if (prefetch_size < max_prefetch_size) {
			pthread_mutex_lock(&prefetch_mutex);
			prefetch_putRequest(bundle.req);
			pthread_cond_signal(&prefetch_get_cond);
			pthread_mutex_unlock(&prefetch_mutex);
		} else {
			destroyRequest(bundle.req);
		}

	}

	while (queue_size > 0) {
		pthread_mutex_lock(&queue_mutex);
		pthread_mutex_lock(&cache_mutex);

		bundle = (*getRequest)();

		pthread_mutex_unlock(&queue_mutex);

		error = processRequest(bundle);

		pthread_mutex_unlock(&cache_mutex);

		++requests_handled;

		pthread_mutex_lock(&log_mutex);
		logRequest(bundle, thread_id, requests_handled, error);
		pthread_mutex_unlock(&log_mutex);

		destroyRequest(bundle.req);
	}

	pthread_exit();
}

void *prefetch_thread(void *ignored) {
	char guessed_filename[1024];
	struct request* req;

	char* data;
	intmax_t filesize;

	while (global_exit == 0) {
		pthread_mutex_lock(&prefetch_mutex);

		assert ( prefetch_size >= 0 );

		if (prefetch_size == 0)
			pthread_cond_wait(&prefetch_get_cond);

		req = prefetch_getRequest();
		assert ( req != NULL );

		pthread_mutex_unlock(&prefetch_mutex);

		if ( guess_next( req->filename, guessed_filename ) == 0 ) {
			data = getFile(guessed_filename);
			filesize = getFileSize(guessed_filename);

			pthread_mutex_lock(&cache_mutex);
			cache_putEntry(  createCacheEntry( guessed_filename, data, filesize )  );
			pthread_mutex_unlock(&cache_mutex);
		}

		destroyRequest(req);
	}

	pthread_exit();
}



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
    if ( chdir( argv[2] ) != 0 ) {
    	fprintf(stderr, "Error! Could not set the web tree root directory to %s", argv[2]);
    	exit(1);
    }

    // argv[3] := <num_dispatch>
    // The number of dispatch threads to start
    int num_dispatch = atoi( argv[3] );
    if (num_dispatch <= 0 || num_dispatch > MAX_DISPATCH_THREADS) {
    	fprintf(stderr, "Error! Invalid number of dispatch threads (Max is %d)\n", MAX_DISPATCH_THREADS);
    	exit(1);
    }

    // argv[4] := <num_workers>
    // The number of worker threads to start
    int num_workers = atoi( argv[4] );
    if (num_workers <= 0 || num_workers > MAX_WORKER_THREADS) {
    	fprintf(stderr, "Error! Invalid number of worker threads (Max is %d)\n", MAX_WORKER_THREADS);
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
    max_queue_size = atoi( argv[6] );
    if (max_queue_size <= 0 || max_queue_size > MAX_REQUEST_QUEUE_LENGTH) {
    	fprintf(stderr, "Error! Invalid request queue length (Max length is %d)\n", MAX_REQUEST_QUEUE_LENGTH);
    	exit(1);
    }

    // argv[7] := <mode>
    // The mode (FCFS, CRF, SFF) to run the worker thread(s) in
    // FCFS: First Come First Serve
    // CRF: Cached Requests First
    // SFF: Smallest File First
    enum mode m = atoi( argv[7] );
    struct request_bundle (*getRequest) () = NULL;

    switch (m) {
    case FCFS:
    	getRequest = &queue_getRequest;
    	break;
    case CRF:
    	getRequest = &getCachedRequest;
    	break;
    case SFF:
    	getRequest = &queue_getSmallRequest;
    	break;
    default:
    	fprintf(stderr, "Error! Invalid mode (Must be 0 for FCFS, 1 for CRF, or 2 for SFF)\n");
    	exit(1);
    }

    // argv[8] := <cache-entries>
    // The size of the cache, in number of entries
    int max_cache_size = atoi( argv[8] );
    if (max_cache_size <= 0 || max_cache_size > MAX_CACHE_SIZE) {
    	fprintf(stderr, "Error! Invalid cache size (Max size is %d)\n", MAX_CACHE_SIZE);
    	exit(1);
    }


    // Initialize request queue, prefetch queue, and cache
    init_lists();

    logfile = fopen("web_server_log", "a");

    assert( logfile != NULL );



    pthread_t dispatch_threads[num_dispatch];
    pthread_t worker_threads[num_workers];
    pthread_t prefetch_threads[num_prefetch];

    int i;
    for (i = 0; i < num_dispatch; ++i)
    	pthread_create(&dispatch_threads[i], NULL, &dispatch_thread, NULL);
    for (i = 0; i < num_workers; ++i)
    	pthread_create(&worker_threads[i], NULL, &worker_thread, NULL);
    for (i = 0; i < num_prefetch; ++i)
    	pthread_create(&prefetch_threads[i], NULL, &prefetch_threads, NULL);

    for (i = 0; i < num_dispatch; ++i)
    	pthread_join(dispatch_threads[i], NULL);

    global_exit = 1;

    for (i = 0; i < num_workers; ++i)
    	pthread_join(worker_threads[i], NULL);
    for (i = 0; i < num_prefetch; ++i)
    	pthread_join(prefetch_threads[i], NULL);

    printf("Shutting down\n");

    return 0;
}

