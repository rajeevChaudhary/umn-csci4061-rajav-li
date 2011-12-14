/* csci4061 F2011 Assignment 4
 * section: 3
 * login: rajav003
 * date: 12/04/11
 * name: Jonathan Rajavuori, Ou Li
 * id: 3438942, 4358557 */




#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdint.h>

#include "util.h"


#define MAX_DISPATCH_THREADS        100
#define MAX_WORKER_THREADS          100
#define MAX_PREFETCH_THREADS        100
#define MAX_CACHE_SIZE              100
#define MAX_REQUEST_QUEUE_LENGTH    100
#define MAX_PREFETCH_QUEUE_LENGTH	100
#define FILENAME_SIZE				1024
#define HASH_SEED					42



enum mode {
	FCFS = 0,
	CRF = 1,
	SFF = 2
};

FILE *logfile; //"web_server_log" in web tree root dir
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// This is set to true when all dispatch threads exit
// It signifies that all other threads should finish their work so the server can exit cleanly
int global_exit = 0;

// From guess.o
int guess_next(char *filename, char *guessed);






// MurmurHash 2 algorithm created by Austin Appleby. Code is in the public domain.
// (Current version is MurmurHash 3; This version is simpler.)
// This hash function is used for keying the filenames of cache entries.
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











//This struct is the type of the main request queue and the prefetch request queue
struct request {
    int fd;
	char filename[FILENAME_SIZE];

	struct request* next;
    struct request* prev;
};

//This struct is used for the cache
struct cache_entry {
    char filename[FILENAME_SIZE];
    char* filedata;
    intmax_t filesize;

    struct cache_entry* next;
    struct cache_entry* prev;

    struct cache_entry* next_collision;
    struct cache_entry* prev_collision;
};

//This struct is used to package request/cache_entry pairs for returning from the
//(*getRequest) family of functions for usage in processRequest
struct request_bundle {
	struct request* req;
	struct cache_entry* ent;
};




struct request *queue_sentinel;
int queue_size;
int max_queue_size; // Set by user, will not exceed MAX_REQUEST_QUEUE_LENGTH
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
// Condition: Queue has items to get
// Checked by worker threads
pthread_cond_t queue_get_cond = PTHREAD_COND_INITIALIZER;
// Condition: Queue has room to put new items
// Checked by dispatch threads
pthread_cond_t queue_put_cond = PTHREAD_COND_INITIALIZER;


struct request *prefetch_sentinel;
int prefetch_size;
int max_prefetch_size = MAX_PREFETCH_QUEUE_LENGTH;
pthread_mutex_t prefetch_mutex = PTHREAD_MUTEX_INITIALIZER;
// Condition: Prefetch queue has items to get
// Checked by prefetch threads
pthread_cond_t prefetch_get_cond = PTHREAD_COND_INITIALIZER;

struct cache_entry *cache_sentinel;
struct cache_entry **cache_hashtable; // Static array of size max_cache_size
int cache_size;
int max_cache_size; // Set by user, will not exceed MAX_CACHE_SIZE
pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;


//SUMMARY
// Initializes the linked lists by allocating the sentinels and setting up their pointers
//NOTES
// This function expects global variable max_cache_size to be set before being called
void init_lists() {
	fprintf(stderr, "init_lists: Called\n");

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

	assert ( queue_sentinel    != NULL &&
			 prefetch_sentinel != NULL &&
			 cache_sentinel    != NULL &&
			 cache_hashtable   != NULL );
}


//SUMMARY
// Allocates and initializes a request
//RETURNS
// A pointer to the created request
struct request* createRequest(int fd, const char* const fn) {
	struct request* req = (struct request*)malloc( sizeof(struct request) );

	assert( req != NULL );

	fprintf(stderr, "createRequest: Allocated new request with fd %d and filename %s\n", fd, fn);

	req->fd = fd;
	strncpy(req->filename, fn, FILENAME_SIZE);

	req->next = NULL;
	req->prev = NULL;

	return req;
}

//SUMMARY
// Deallocates request 'req'
void destroyRequest(struct request* req) {
	fprintf(stderr, "destroyRequest: Deallocating request with fd %d and filename %s\n", req->fd, req->filename);
	free( req );
}



//SUMMARY
// Allocates and initializes a cache entry
//RETURNS
// A pointer to the created request
struct cache_entry* createCacheEntry(const char* const fn, char* data, intmax_t fs) {
	struct cache_entry* ent = (struct cache_entry*)malloc( sizeof(struct cache_entry) );

	assert( ent != NULL );
	assert( data != NULL );

	fprintf(stderr, "createCacheEntry: Allocating new cache entry with filename %s and filesize %jd\n", fn, fs);

	strncpy(ent->filename, fn, FILENAME_SIZE);
	ent->filedata = data;
	ent->filesize = fs;

	ent->next = NULL;
	ent->prev = NULL;
	ent->next_collision = NULL;
	ent->prev_collision = NULL;

	return ent;
}

//SUMMARY
// Deallocates cache entry 'ent' and its contained data
void destroyCacheEntry(struct cache_entry* ent) {
	fprintf(stderr, "destroyCacheEntry: Deallocating cache entry with filename %s and filesize %jd\n", ent->filename, ent->filesize);
	free( ent->filedata );
	free( ent );
}








//////// Internal linked list operations on the request queue

//SUMMARY
// Add request 'r' to the end of the queue
void q_add(struct request* r) {

	fprintf(stderr, "q_add: Called\n");

	assert ( r != NULL );

	queue_sentinel->prev->next = r;
	r->prev = queue_sentinel->prev;
	queue_sentinel->prev = r;
	r->next = queue_sentinel;

	++queue_size;

}

//SUMMARY
// Remove request 'r' from the queue
void q_remove(struct request* r) {

	fprintf(stderr, "q_remove: Called\n");

	assert ( r->prev != NULL && r->next != NULL );

	if (r != queue_sentinel) {
		r->prev->next = r->next;
		r->next->prev = r->prev;

		--queue_size;
	}

}

//SUMMARY
// Return the first request added to the queue or NULL if the queue is empty
struct request* q_first() {

	fprintf(stderr, "q_first: Called\n");

	struct request* head = queue_sentinel->next;

	assert ( head != NULL );

	if (head != queue_sentinel)
		return head;
	else
		return NULL;

}

//SUMMARY
// Return the request following request 'r' in the queue or NULL if it is the last request
struct request* q_nextOf(struct request* r) {

	fprintf(stderr, "q_nextOf: Called\n");

	r = r->next;

	assert( r != NULL );

	if (r == queue_sentinel)
		return NULL;
	else
		return r;

}

//SUMMARY
// Return the first request in the queue after removing it from the queue, or NULL if the queue is empty
struct request* q_shift() {

	fprintf(stderr, "q_shift: Called\n");

	struct request* first = q_first();

	if (first != NULL)
		q_remove(first);

	return first;

}





//////// Internal linked list operations on the prefetch queue

//SUMMARY
// Add request 'r' to the end of the queue
void p_add(struct request* r) {

	fprintf(stderr, "p_add: Called\n");

	assert ( r != NULL );

	prefetch_sentinel->prev->next = r;
	r->prev = prefetch_sentinel->prev;
	prefetch_sentinel->prev = r;
	r->next = prefetch_sentinel;

	++prefetch_size;

}

//SUMMARY
// Remove request 'r' from the queue
void p_remove(struct request* r) {

	fprintf(stderr, "p_remove: Called\n");

	assert ( r->prev != NULL && r->next != NULL );

	if (r != prefetch_sentinel) {
		r->prev->next = r->next;
		r->next->prev = r->prev;

		--prefetch_size;
	}

}

//SUMMARY
// Return the first request added to the queue or NULL if the queue is empty
struct request* p_first() {

	fprintf(stderr, "p_first: Called\n");

	struct request* head = prefetch_sentinel->next;

	assert ( head != NULL );

	if (head == prefetch_sentinel)
		return NULL;
	else
		return head;

}

//SUMMARY
// Return the request following request 'r' in the queue or NULL if it is the last request
struct request* p_nextOf(struct request* r) {

	fprintf(stderr, "p_nextOf: Called\n");

	r = r->next;

	assert( r != NULL );

	if (r == prefetch_sentinel)
		return NULL;
	else
		return r;

}

//SUMMARY
// Return the first request in the queue after removing it from the queue, or NULL if the queue is empty
struct request* p_shift() {

	fprintf(stderr, "p_shift: Called\n");

	struct request* first = p_first();

	if (first != NULL)
		p_remove(first);

	return first;

}






//////// Internal hashed linked list operations on the cache

//SUMMARY
// Insert cache entry 'e' into the hash table, accounting for collisions
void hash_add(struct cache_entry* e) {

	fprintf(stderr, "hash_add: Called\n");

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

//SUMMARY
// Remove cache entry 'e' from the hash table
void hash_remove(struct cache_entry* e) {

	if (e->prev_collision == NULL) { // This is the first entry in the collision list and the one referenced by the array index
		int hash = MurmurHash2(e->filename, FILENAME_SIZE, HASH_SEED) % max_cache_size;

		fprintf(stderr, "hash_remove: Hash generated for filename %s: %d\n", e->filename, hash);

		if (e->next_collision == NULL) // This hash index is now empty
			cache_hashtable[hash] = NULL;
		else
			cache_hashtable[hash] = e->next_collision;
	} else {
		e->prev_collision->next_collision = e->next_collision;
		if (e->next_collision != NULL)
			e->next_collision->prev_collision = e->prev_collision;
	}

}

//SUMMARY
// Return the cache entry with filename key 'fn' or NULL if no such entry exists
//NOTES
// This key will uniquely map to one cache entry due to the program flow --
// If this search keys to a cache entry and returns non-NULL, then no cache entry will be added in that line of execution
struct cache_entry* hash_search(const char* const fn) {

	int hash = MurmurHash2(fn, FILENAME_SIZE, HASH_SEED) % max_cache_size;

	fprintf(stderr, "hash_search: Hash generated for filename %s: %d\n", fn, hash);

	struct cache_entry* ent = cache_hashtable[hash];

	while (ent != NULL && strcmp(ent->filename, fn) != 0)
		ent = ent->next_collision;

	return cache_hashtable[hash];

}

//SUMMARY
// Add entry 'e' to the end of the cache
//NOTES
// This function will call hash_add to add the same entry to the hash table
void c_add(struct cache_entry* e) {

	fprintf(stderr, "c_add: Called\n");

	cache_sentinel->prev->next = e;
	e->prev = cache_sentinel->prev;
	cache_sentinel->prev = e;
	e->next = cache_sentinel;

	++cache_size;

	hash_add(e);

}

//SUMMARY
// Remove entry 'e' from the cache
//NOTES
// This function will call hash_remove to remove the same entry from the hash table
void c_remove(struct cache_entry* e) {

	fprintf(stderr, "c_remove: Called\n");

	assert ( e->prev != NULL && e->next != NULL );

	if (e != cache_sentinel) {
		e->prev->next = e->next;
		e->next->prev = e->prev;

		--cache_size;
	}

	hash_remove(e);

}

//SUMMARY
// Return the first entry add to the cache or NULL if the cache is empty
struct cache_entry* c_first() {

	fprintf(stderr, "c_first: Called\n");

	struct cache_entry* head = cache_sentinel->next;

	assert( head != NULL );

	if (head == cache_sentinel)
		return NULL;
	else
		return head;

}

//SUMMARY
// Return the entry following 'e' in the cache or NULL if it is the last entry
//NOTES
// This function is never called in this code, since there is no need to iterate over the cache.
// It is included for completeness.
struct cache_entry* c_nextOf(struct cache_entry* e) {

	fprintf(stderr, "c_nextOf: Called\n");

	e = e->next;

	assert( e != NULL );

	if (e == cache_sentinel)
		return NULL;
	else
		return e;

}

//SUMMARY
// Return the first request in the cache after removing it from the cache, or NULL if the cache is empty
struct cache_entry* c_shift() {

	fprintf(stderr, "c_shift: Called\n");

	struct cache_entry* first = c_first();

	if (first != NULL)
		c_remove(first);

	return first;

}



//////// File operations

//SUMMARY
// Reads in file at path 'filename'
//RETURNS
// A char (bytewise) pointer to the data in memory or NULL on failure
char * getFile(const char* filename) {

	fprintf(stderr, "getFile: Getting file %s\n", filename);

	FILE* file = fopen(filename, "rb");
	intmax_t fileLength;

	if (file == NULL)
		return NULL; //Error

	fprintf(stderr, "getFile: File opened successfully\n");

	fseek(file, 0, SEEK_END);
	fileLength = ftell(file);
	fseek(file, 0, SEEK_SET);

	fprintf(stderr, "getFile: File seek succeeded, length retrieved: %jd\n", fileLength);

	char* data = (char *)malloc( fileLength + 1 );

	assert( data != NULL );

	assert( fread(data, fileLength, 1, file) == 1 );
	fclose(file);

	return data;
}

//SUMMARY
// Returns the file size of file 'filename'
intmax_t getFileSize(const char* filename) {

	fprintf(stderr, "getFileSize: Getting size of file %s\n", filename);

	struct stat stat_buf;

	if ( stat(filename, &stat_buf) == 0 ) {
		fprintf(stderr, "getFileSize: Size gotten: %jd\n", (intmax_t)stat_buf.st_size);
		return (intmax_t)stat_buf.st_size;
	}
	else
		return -1;
}

char content_type_return_val[50];

//SUMMARY
// Returns the content type of the file at 'filename'
char* getFileContentType(const char* const filename) {
	fprintf(stderr, "getFileContentType: Detecting content type of file %s\n", filename);

	const char* extension = strrchr(filename, '.');
	if (extension == NULL)
		strcpy(content_type_return_val, "text/plain");

	++extension;

	if (strcmp(extension, "htm") == 0 || strcmp(extension, "html") == 0)
		strcpy(content_type_return_val, "text/html");
	else if (strcmp(extension, "gif") == 0)
		strcpy(content_type_return_val, "image/gif");
	else if (strcmp(extension, "jpg") == 0 || strcmp(extension, "jpeg") == 0)
		strcpy(content_type_return_val, "image/jpeg");
	else
		strcpy(content_type_return_val, "text/plain");

	return content_type_return_val;
}





//////// Interface operations on the request queue, prefetch queue, and the cache

//SUMMARY
// Adds the cache entry 'ent' to the end
//NOTES
// Must be locked over the cache - can be unlocked immediately following the call
// No other synchronization needed, since old entries will automatically be displaced once cache is full
void cache_putEntry(struct cache_entry* ent) {

	assert( ent->filedata != NULL );

	fprintf(stderr, "cache_putEntry: Adding entry for filename %s\n", ent->filename);

	assert( cache_size <= max_cache_size );

	if (cache_size == max_cache_size) {
		fprintf(stderr, "cache_putEntry: Cache full, shifting (%d, %d)\n", cache_size, max_cache_size);
		destroyCacheEntry( c_shift() ); // This will also free the file data stored in the entry
	}

	c_add(ent);

}

//SUMMARY
// Searches the cache for entry matching filename 'fn'
//RETURNS
// A pointer to the first cache entry matching the given filename or NULL if no such entry exists
//NOTES
// Must be locked over the cache INCLUDING ALL USAGE OF THE RETURNED CACHE ENTRY
// The cache entry REMAINS IN THE CACHE
// The returned cache entry should NOT be destroyed upon falling out of the caller's context
struct cache_entry* cache_search(const char* const fn) {

	fprintf(stderr, "cache_search: Getting entry for filename %s\n", fn);

	return hash_search(fn);

}


// Must be locked over the queue - can be unlocked immediately following the call
// Must be synchronized over the "queue put" semaphore
// The "queue get" semaphore should be incremented following the call
void queue_putRequest(struct request* req) {

	fprintf(stderr, "queue_putRequest: Adding request for connection %d with filename %s\n", req->fd, req->filename);

	q_add(req);

}

// Must be locked over the queue - can be unlocked immediately following the call
// Must be synchronized over the "queue get" semaphore
// The "queue put" semaphore should be incremented following the call
// The returned request is freed from the list and must be destroyed before it goes out of context
struct request_bundle queue_getRequest() {

	fprintf(stderr, "queue_getRequest: Called\n");

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

	fprintf(stderr, "queue_getSmallRequest: Called\n");

	struct request* req = q_first();
	if (req == NULL) {
		fprintf(stderr, "queue_getSmallRequest: Error! Queue held no requests!\n");

		struct request_bundle bundle = { NULL, NULL };
		return bundle;
	}

	//assert (queue_size != 0);

	struct request* small = req;
	intmax_t smallsize = getFileSize(req->filename);

	while ( (req = q_nextOf(req)) != NULL ) {
		intmax_t thissize = getFileSize(req->filename);
		if (thissize < smallsize) {
			small = req;
			smallsize = thissize;
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

	fprintf(stderr, "prefetch_putRequest: Called\n");

	p_add(req);

}

// Must be locked over the prefetch queue - can be unlocked immediately following the call
// The returned request is freed from the list and must be destroyed before it goes out of context
// Must be synchronized over the "prefetch get" semaphore
struct request* prefetch_getRequest() {

	fprintf(stderr, "prefetch_getRequest: Called\n");

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

	fprintf(stderr, "getCachedRequest: Called\n");

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
		req = q_shift();

	struct request_bundle bundle = { req, ent };

	return bundle;

}



// Must be synchronized over the log mutex
void logRequest(struct request_bundle bundle, int thread_id, int requests_handled, int cache_hit, const char* error) {
	fprintf(stderr, "logRequest: Logging request\n");
	fprintf(logfile,"[%d][%d][%d][%s][%s]",
			thread_id,
			requests_handled,
			bundle.req->fd,
			cache_hit == 0 ? "MISS" : "HIT",
			bundle.req->filename);
	assert (bundle.ent != NULL);

	if (error == NULL)
		fprintf(logfile, "[%jd]\n", bundle.ent->filesize);
	else
		fprintf(logfile, "[%s]\n", error);

	fflush(logfile);

	fprintf(stderr, "logRequest: Done logging request\n");
}

const char* process_request(struct request_bundle* bundle, int* cache_hit) {

	fprintf(stderr, "process_request: Processing request on connection %d with filename %s\n", bundle->req->fd, bundle->req->filename);

	assert( bundle->req != NULL );

	char* filename = bundle->req->filename;
	char* data;
	intmax_t filesize;

	char* error = NULL;

	if (bundle->ent != NULL) {
		*cache_hit = 1;
		fprintf(stderr, "process_request: Cache hit\n");
		data = bundle->ent->filedata;
		filesize = bundle->ent->filesize;
	} else {
		*cache_hit = 0;
		fprintf(stderr, "process_request: Cache miss, getting file\n");
		data = getFile(filename);

		if (data == NULL)
			error = "Error reading file";
		else {
			filesize = getFileSize(filename);

			if (filesize == -1)
				error = "Error getting file size";
			else {
				struct cache_entry* ent = createCacheEntry( filename, data, filesize );
				cache_putEntry(ent);
				bundle->ent = ent;
			}

			// At this point, the request filesize and entry filesize should match up... *fingers crossed*
		}
	}

	if (error == NULL) {
		int fs = filesize;
		char* contentType = getFileContentType(filename);
		fprintf(stderr, "process_request: Calling return_result\n");
		assert ( data != NULL );
		return_result( bundle->req->fd, contentType, data, fs );
	}
	else
		return_error( bundle->req->fd, error );

	return error;

}


void *dispatch_thread(void * ignored) {
	int fd;
	char filename_buffer[FILENAME_SIZE + 1];
	char* filename;
	struct request* req;

	fprintf(stderr, "dispatch_thread: Starting up\n");

	while ( (fd = accept_connection()) >= 0 ) {
		fprintf(stderr,"dispatch_thread: Got connection: %d\n", fd);
		if (get_request(fd, filename_buffer) == 0) {
			fprintf(stderr, "dispatch_thread: Got request for: %s\n", filename_buffer);

			if (filename_buffer[0] == '/') {
				fprintf(stderr, "dispatch_thread: Trimming leading / from filename\n");

				filename = &filename_buffer[1];
			} else
				filename = &filename_buffer[0];

			fprintf(stderr, "dispatch_thread: Acting on file: %s\n", filename);

			req = createRequest(fd, filename);

			pthread_mutex_lock(&queue_mutex);

			assert(queue_size <= max_queue_size);

			if (queue_size == max_queue_size)
				pthread_cond_wait(&queue_put_cond, &queue_mutex);

			queue_putRequest(req);

			pthread_cond_signal(&queue_get_cond);

			pthread_mutex_unlock(&queue_mutex);
		}
	}

	fprintf(stderr, "dispatch_thread: Shutting down\n");

	return NULL;
}

struct request_bundle (*getRequest)() = NULL;

void *worker_thread(void * id) {
	struct request_bundle bundle;
	int thread_id = *((int*)id);
	int requests_handled = 0;
	int cache_hit;
	const char* error;

	fprintf(stderr, "worker_thread: Starting up\n");

	while (global_exit == 0) {
		fprintf(stderr, "worker_thread: Iterating\n");

		pthread_mutex_lock(&queue_mutex);

		fprintf(stderr, "worker_thread: Queue lock acquired\n");

		assert (queue_size >= 0);

		if (queue_size == 0) {
			fprintf(stderr, "worker_thread: Queue empty, waiting for condition 'queue get' and releasing queue lock\n");
			pthread_cond_wait(&queue_get_cond, &queue_mutex);
			fprintf(stderr, "worker_thread: Condition 'queue get' fufilled, queue lock reacquired\n");
		}

		pthread_mutex_lock(&cache_mutex);

		fprintf(stderr, "worker_thread: Cache lock acquired\n");

		assert ( getRequest != NULL );

		bundle = (*getRequest)();

		fprintf(stderr, "worker_thread: Got request for connection %d with filename %s\n", bundle.req->fd, bundle.req->filename);

		pthread_cond_signal(&queue_put_cond);
		pthread_mutex_unlock(&queue_mutex);

		error = process_request(&bundle, &cache_hit);

		fprintf(stderr, "worker_thread: Done processing request\n");

		++requests_handled;

		pthread_mutex_lock(&log_mutex);
		logRequest(bundle, thread_id, requests_handled, cache_hit, error);
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

	fprintf(stderr, "worker_thread: Should exit, starting to empty request queue\n");

	while (queue_size > 0) {
		pthread_mutex_lock(&queue_mutex);
		pthread_mutex_lock(&cache_mutex);

		bundle = (*getRequest)();

		pthread_mutex_unlock(&queue_mutex);

		error = process_request(&bundle, &cache_hit);

		pthread_mutex_unlock(&cache_mutex);

		++requests_handled;

		pthread_mutex_lock(&log_mutex);
		logRequest(bundle, thread_id, requests_handled, cache_hit, error);
		pthread_mutex_unlock(&log_mutex);

		destroyRequest(bundle.req);
	}

	fprintf(stderr, "worker_thread: Shutting down\n");

	return NULL;
}

void *prefetch_thread(void *ignored) {
	char guessed_filename[1024];
	struct request* req;

	char* data;
	intmax_t filesize;

	fprintf(stderr, "prefetch_thread: Starting up\n");

	while (global_exit == 0) {
		pthread_mutex_lock(&prefetch_mutex);

		assert ( prefetch_size >= 0 );

		if (prefetch_size == 0)
			pthread_cond_wait(&prefetch_get_cond, &prefetch_mutex);

		req = prefetch_getRequest();
		assert ( req != NULL );

		fprintf(stderr, "prefetch_thread: Processing request with filename %s\n", req->filename);

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

	fprintf(stderr, "prefetch_thread: Shutting down\n");

	return NULL;
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
    max_cache_size = atoi( argv[8] );
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

    int worker_thread_ids[num_workers];
    int i;

    for (i = 0; i < num_workers; ++i)
    	worker_thread_ids[i] = i + 1;

    for (i = 0; i < num_dispatch; ++i)
    	pthread_create(&dispatch_threads[i], NULL, &dispatch_thread, NULL);
    for (i = 0; i < num_workers; ++i)
    	pthread_create(&worker_threads[i], NULL, &worker_thread, (void*)&worker_thread_ids[i]);
    for (i = 0; i < num_prefetch; ++i)
    	pthread_create(&prefetch_threads[i], NULL, &prefetch_thread, NULL);

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

