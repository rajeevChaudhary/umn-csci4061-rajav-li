#include <sys/time.h>
#include <signal.h>

#include <stdio.h>
#include <stdlib.h>

#define INTERVAL 0
#define INTERVAL_USEC 800000
#define SZ 64
#define how 8

struct node {
    void* address;
    int size;
    bool isFree;
    
    node * next;
    node * prev;
};

struct mm_t {
  void *start;
  node *chunk_list;
  int total_size; 
  //int partitions;
  int num_free_bytes; 
};


int  mm_init (mm_t *MM, int tsz);
void* mm_get (mm_t *MM, int neededSize);
void mm_put (mm_t *MM, void *chunk);
void  mm_release (mm_t *MM);
double comp_time (struct timeval times, struct timeval timee);
