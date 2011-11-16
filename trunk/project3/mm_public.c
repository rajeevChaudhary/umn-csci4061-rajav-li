#include "mm_public.h"

/* Return usec */
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

int mm_init (mm_t *MM, int total_size){
    
    if (MM == NULL || total_size < 1)
        return -1;
    
    MM->start = malloc(total_size);
    if (MM->start == NULL)
        return -1;
    
    MM->catalog = (int*) malloc( sizeof(int)*total_size );
    if (MM->catalog == NULL) {
        free(MM->start);
        return -1;
    } 
    
    MM->size = total_size;
    
    // Initialize node list with one node containing the entire space

    MM->catalog[0] = total_size;
    
    return 0;
    
}

void* mm_get (mm_t *MM, int neededSize) {
    
    if (MM == NULL || neededSize < 1 || MM->size < neededSize)
        return NULL;
    
    int n = 0;
    
    while (MM->catalog[n] < neededSize && n < MM->size) {
        if (MM->catalog[n] > 0)
            n += MM->catalog[n];
        else
            n -= MM->catalog[n];
    }
    
    if (MM->catalog[n] < neededSize)
        return NULL;
    
    int oldChunkSize = MM->catalog[n];
    MM->catalog[n] = MM->catalog[n + neededSize - 1] = -(neededSize);
    
    if (oldChunkSize > neededSize)
        MM->catalog[n + neededSize] = oldChunkSize - neededSize;
    
    return ((unsigned char*) MM->start) + n;
    
}

void mm_put (mm_t *MM, void *chunk) {
    
    if (MM == NULL || chunk == NULL)
        return;
    
    int index = ((unsigned char*) chunk) - ((unsigned char*) MM->start);

    if (index >= MM->size)
        return;
    
    
    int newIndex = index;
    int newSize = -(MM->catalog[index]);
    
    if (index > 0) {
        int left = MM->catalog[index - 1];
        if (left > 0) {
            newIndex -= left;
            newSize += left;
        }
    }
    
    if (index + newSize < MM->size && MM->catalog[index + newSize] > 0)
        newSize += MM->catalog[index + newSize];
    
    MM->catalog[newIndex] = MM->catalog[newIndex + newSize - 1] = newSize;
    
}

void  mm_release (mm_t *MM) {
    free(MM->start);
    free(MM->catalog);
}
