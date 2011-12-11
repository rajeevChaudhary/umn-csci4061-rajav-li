#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include "util.h"

#define MAX_THREADS 100
#define MAX_QLEN 100
#define MAX_CE 100

// structs:
typedef struct request_queue {
   int fd;
   char *request;
} request_queue_t;

typedef struct cache_entry {
   char *request;
   void *memory;
   int size;
   int timestamp;
   int valid;
} cache_entry_t;

// globals:
FILE *logfile;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;
cache_entry_t cache[MAX_CE];
int cache_insert_index = 0; // for 'oldest' replacement method

pthread_mutex_t req_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t req_queue_notfull = PTHREAD_COND_INITIALIZER; // wait until queue isn't full (dispatch)
pthread_cond_t req_queue_notempty = PTHREAD_COND_INITIALIZER; // wait until queue isn't empty (worker)
request_queue_t requests[MAX_QLEN];
int req_remove_index = 0;
int req_insert_index = 0;
int req_num = 0;

int global_timestamp = 0;

int qlen;
int caching;
int cache_entries;

void * dispatch(void *arg) {

   int id = (int)arg;
   int fd;
   char mybuf[1024];
   char *reqptr;

   // printf("Dispatch %d started\n",id);

   while (1) {

      // printf("Thread %d waiting for request...\n",id);
      fd = accept_connection();

      if (fd < 0) {
         return NULL;
      }

      if (get_request(fd,mybuf) != 0) {
         // printf("Bad request, thread id=%d\n",id);
         continue; // back to top of the loop!
      }
      // printf("Dispatch %d got request: %s\n",id,mybuf);
      reqptr = (char *)malloc(strlen(mybuf)+1);
      strcpy(reqptr,mybuf);

      // put request in queue
      pthread_mutex_lock(&req_queue_mutex);
      while (req_num == qlen) // the queue is full!
         pthread_cond_wait(&req_queue_notfull,&req_queue_mutex);

      requests[req_insert_index].fd = fd;
      requests[req_insert_index].request = reqptr;

      req_num++;
      req_insert_index = (req_insert_index + 1) % qlen; // ring buffer

      // printf("disp[%d] inserted req into queue: %s\n",id,reqptr);

      pthread_cond_signal(&req_queue_notempty);
      pthread_mutex_unlock(&req_queue_mutex);

   }
   return NULL;
}

void * worker(void *arg) {

   int id = (int)arg;
   int fd, i, found, filesize, myreqnum, lru_replace_index, lru_ts_min;
   void * memory;
   char *hit_or_miss;
   char mybuf[1024];
   myreqnum = 0;

   while (1) {

      // get request from queue
      pthread_mutex_lock(&req_queue_mutex);
      while (req_num == 0) // the queue is empty!
         pthread_cond_wait(&req_queue_notempty,&req_queue_mutex);

      fd = requests[req_remove_index].fd;
      if (fd == -9999) { // special meaning to this 'request': we must exit!!!
         pthread_mutex_unlock(&req_queue_mutex); // dont forget to unlock first!
         return NULL;
      }

      strcpy(mybuf,requests[req_remove_index].request);
      free(requests[req_remove_index].request);

      req_num--;
      req_remove_index = (req_remove_index + 1) % qlen; // ring buffer

      pthread_cond_signal(&req_queue_notfull);
      pthread_mutex_unlock(&req_queue_mutex);
      myreqnum++;

      if (strcmp(mybuf,"/")==0)
         strcpy(mybuf,"/index.html");

      pthread_mutex_lock(&cache_mutex);

      // increment global timestamp
      global_timestamp++;

      // first, look for the cache entry to exist already
      // (also do some LRU investigation on possible replacement positions)
      found = -1;
      lru_replace_index = -1;
      lru_ts_min = -1;
      for (i=0; i < cache_entries; i++) {
         // LRU STUFF
         if (cache[i].valid == 0) {
            lru_replace_index = i; // save an index to the empty spot
            lru_ts_min = 0; // say it has a timestamp of 0, which can never be beaten
         } else if (cache[i].timestamp < lru_ts_min || lru_ts_min == -1) {
            lru_replace_index = i; // going to replace into this index
            lru_ts_min = cache[i].timestamp; // remember the timestamp of the replace-node
         }
         // FIND-IT-IN-THE-CACHE STUFF
         if (cache[i].valid == 1 && strcmp(mybuf,cache[i].request) == 0) {
            found = i;
         }
      }
      if (found == -1) { // we didn't find it
         hit_or_miss = "MISS";
         // load the file into memory first.

         int filefd;
         if ((filefd = open(mybuf+1,O_RDONLY)) == -1) {
            return_error(fd,"File not found.");
            pthread_mutex_lock(&log_mutex);
            fprintf(logfile,"[%d][%d][%d][%s][%s][%s]\n",id,myreqnum,fd,hit_or_miss,mybuf,"File not found.");
            fflush(logfile);
            pthread_mutex_unlock(&log_mutex);
            pthread_mutex_unlock(&cache_mutex); // dont forget to unlock
            continue; // next request => top of while loop
         }

         struct stat st;
         fstat(filefd,&st);
         filesize = st.st_size;
         memory = malloc(filesize);
         read(filefd,memory,filesize);
         close(filefd);

         if (caching == 1) { // 'oldest' replacement

            if (cache[cache_insert_index].valid == 1) {
               free(cache[cache_insert_index].memory);
               free(cache[cache_insert_index].request);
            }

            char *reqstr = (char *)malloc(strlen(mybuf)+1);
            strcpy(reqstr,mybuf);
            cache[cache_insert_index].valid = 1;
            cache[cache_insert_index].memory = memory;
            cache[cache_insert_index].request = reqstr;
            cache[cache_insert_index].size = filesize;
            cache_insert_index = (cache_insert_index + 1) % cache_entries;

         } else if (caching == 2) { // LRU replacement

            if (cache[lru_replace_index].valid == 1) {
               free(cache[lru_replace_index].memory);
               free(cache[lru_replace_index].request);
            }

            char *reqstr = (char *)malloc(strlen(mybuf)+1);
            strcpy(reqstr,mybuf);
            cache[lru_replace_index].valid = 1;
            cache[lru_replace_index].memory = memory;
            cache[lru_replace_index].request = reqstr;
            cache[lru_replace_index].size = filesize;
            cache[lru_replace_index].timestamp = global_timestamp;

         }
      } else {
         // we found it in the cache already!
         hit_or_miss = "HIT";
         memory = cache[found].memory;
         filesize = cache[found].size;
         cache[found].timestamp = global_timestamp;
      }
      pthread_mutex_lock(&log_mutex);
      fprintf(logfile,"[%d][%d][%d][%s][%s][%d]\n",id,myreqnum,fd,hit_or_miss,mybuf,filesize);
      fflush(logfile);
      pthread_mutex_unlock(&log_mutex);

      int reqlen = strlen(mybuf);
      char * contenttype;
      if (strcmp(mybuf+reqlen-5,".html")==0) {
         contenttype = "text/html";
      } else if (strcmp(mybuf+reqlen-4,".jpg")==0) {
         contenttype = "image/jpeg";
      } else if (strcmp(mybuf+reqlen-4,".gif")==0) {
         contenttype = "image/gif";
      } else {
         contenttype = "text/plain";
      }

      if (return_result(fd,contenttype,memory,filesize) != 0) {
         printf("Couldn't return result, thread id=%d\n",id);
      }

      pthread_mutex_unlock(&cache_mutex); // must wait until here to unlock (protect: memory variable)

      if (caching == 0)
         free(memory);

   }
   return NULL;
}

double comp_time (struct timeval times, struct timeval timee)
{

  double elap = 0.0;

  if (timee.tv_sec > times.tv_sec) {
    elap += (((double)(timee.tv_sec - times.tv_sec -1))*1000000.0);
    elap += timee.tv_usec + (1000000-times.tv_usec);
  }
  else {
    elap = timee.tv_usec - times.tv_usec;
  }
  return ((unsigned long)(elap));

}

int main(int argc, char **argv) {

   pthread_t dispatch_threads[MAX_THREADS];
   pthread_t worker_threads[MAX_THREADS];

   logfile = fopen("webserver_log","w");

   if (argc != 8) {
      printf("usage: %s port path num_dispatch num_workers qlen caching cache-entries\n",argv[0]);
      return -1;
   }

   if (chdir(argv[2])!=0) {
      perror("couldn't change directory to server root");
      return -1;
   }

   int port = atoi(argv[1]);
   int num_dispatch = atoi(argv[3]);
   int num_worker = atoi(argv[4]);
   qlen = atoi(argv[5]);
   caching = atoi(argv[6]);
   cache_entries = atoi(argv[7]);

   init(port);

   if (num_dispatch > MAX_THREADS || num_dispatch < 1) {
      fprintf(stderr,"Invalid number of dispatch threads.\n");
      return -2;
   }
   if (num_worker > MAX_THREADS || num_worker < 1) {
      fprintf(stderr,"Invalid number of worker threads.\n");
      return -2;
   }
   if (qlen > MAX_QLEN || qlen < 1) {
      fprintf(stderr,"Invalid qlen size.\n");
      return -2;
   }
   if (caching != 0 && caching != 1 && caching != 2) {
      fprintf(stderr,"Invalid caching parameter.\n");
      return -2;
   }
   if (cache_entries > MAX_CE || cache_entries < 1) {
      fprintf(stderr,"Invalid cache-entries value.\n");
      return -2;
   }
   int i;
   for (i=0; i < cache_entries; i++)
      cache[i].valid = 0;

   printf("Starting server on port %d: %d disp, %d work\n",port,num_dispatch,num_worker);

   struct timeval time_start, time_end;
   int j;

   /* start timer */
   j = gettimeofday (&time_start, (void *)NULL);


   for (i=0; i < num_worker; i++) {
      pthread_create(worker_threads+i,NULL,worker,(void *)i);
   }
   for (i=0; i < num_dispatch; i++) {
      pthread_create(dispatch_threads+i,NULL,dispatch,(void *)i);
   }

   for (i=0; i < num_dispatch; i++) {
      pthread_join(dispatch_threads[i],NULL);
   }
   // at this point, all the dispatch threads have managed to exit.
   // the request queue could be empty, non-empty, and/or the workers
   //    could be busy doing stuff right now.
   // => insert a 'dummy' item into the request queue, so that when workers
   //    receive it, it means that they should exit.
   pthread_mutex_lock(&req_queue_mutex);
   while (req_num == qlen) // the queue is full!
      pthread_cond_wait(&req_queue_notfull,&req_queue_mutex);
   requests[req_insert_index].fd = -9999;
   requests[req_insert_index].request = NULL;
   req_num++;
   req_insert_index = (req_insert_index + 1) % qlen; // ring buffer
   pthread_cond_broadcast(&req_queue_notempty); // wake EVERYONE UP!!
   pthread_mutex_unlock(&req_queue_mutex);

   // the workers should be able to exit now.
   for (i=0; i < num_worker; i++) {
      pthread_join(worker_threads[i],NULL);
   }

   fclose(logfile);
   printf("all threads have exited, main thread exiting...\n");
    
   j = gettimeofday (&time_end, (void *)NULL);

   fprintf (stderr, "Time taken =  %f msec\n",
	     comp_time (time_start, time_end)/1000.0);
   return 0;

}

