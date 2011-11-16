/* CSci4061 F2011 Assignment 3
 * section: 3
 * login: rajav003
 * date: 11/16/11
 * name: Jonathan Rajavuori, Ou Li
 * id: 3438942, 4358557 */

// This memory manager works by allocating an int array
// of the same size as the memory needed, in order to
// catalog the current size of each "chunk" in the memory
// as well as whether or not it is free (positive == free).
//
// When chunks are returned to the manager, any free chunks
// to the left or right of the chunk are merged into it,
// so that there is never more than one consecutive free
// chunk in memory.
//
// Overall, this manager runs much faster than malloc/free.
// mm_get runs in worst case linear time on the total size of
// the memory (if filled with 1-byte chunks), but in a very
// good average time. mm_put runs in constant time.

// Note: The catalog will also store the current size of each
// chunk at the end of its mirror chunk in the catalog, in
// order to support constant-time mm_put with merging.

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


// Initialize a dynamic memory manager in struct MM with total_size bytes of space
// Returns 0 on success and -1 on failure
int mm_init (mm_t *MM, int total_size){
    
    // Fail if arguments don't make sense
    if (MM == NULL || total_size < 1)
        return -1;
    
    MM->start = malloc(total_size);
    // Fail if malloc failed
    if (MM->start == NULL)
        return -1;
    
    MM->catalog = (int*) malloc( sizeof(int)*total_size );
    // Fail if malloc failed
    if (MM->catalog == NULL) {
        free(MM->start);
        return -1;
    } 
    
    // Store the total_size for bound checking
    MM->size = total_size;
    
    // Initially just one chunk with full size
    MM->catalog[0] = total_size;
    
    return 0;
    
}

// Get a chunk of byte size neededSize from memory manager initialized
// to MM.
// Returns a pointer to the chunk or NULL on failure.
void* mm_get (mm_t *MM, int neededSize) {
    
    // Fail if the arguments don't make sense
    if (MM == NULL || neededSize < 1 || MM->size < neededSize)
        return NULL;
    
    // n is iterator over the current chunks (free or not) in MM
    int n = 0;
    
    // while n is not free (negative) or not big enough
    // AND n hasn't fallen off the end of MM
    while (MM->catalog[n] < neededSize && n < MM->size) {
        // Iterate to the next chunk
        if (MM->catalog[n] > 0)
            n += MM->catalog[n];
        else
            // We have to take the negative of a non-free chunk
            n -= MM->catalog[n];
    }
    
    // Fail if we fell off the end without finding a chunk big enough
    if (MM->catalog[n] < neededSize)
        return NULL;
    
    int oldChunkSize = MM->catalog[n];
    // Splice out the new chunk, updating the catalog...
    MM->catalog[n] = MM->catalog[n + neededSize - 1] = -(neededSize);
    
    // ... and if necessary, update for the new free space at the end
    //     of the old chunk, too
    if (oldChunkSize > neededSize)
        MM->catalog[n + neededSize] = oldChunkSize - neededSize;
    
    // Return the chunk's pointer
    return ((unsigned char*) MM->start) + n;
    
}

// Return the given chunk to the memory manager, freeing it up
void mm_put (mm_t *MM, void *chunk) {
    
    // End if the arguments don't make sense
    if (MM == NULL || chunk == NULL)
        return;
    
    // Convert the pointers back to a catalog index
    int index = ((unsigned char*) chunk) - ((unsigned char*) MM->start);

    // End if the argument didn't make sense
    if (index >= MM->size)
        return;
    
    // Default to updating that index with a flipped sign
    int newIndex = index;
    int newSize = -(MM->catalog[index]);
    
    // If this isn't the first chunk in the MM...
    if (index > 0) {
        // Update the next chunk to the left instead...
        int left = MM->catalog[index - 1];
        if (left > 0) { // ... as long as it's free, of course
            newIndex -= left;
            newSize += left;
        }
    }
    
    // Also include the next chunk to right if it exists and is free
    if (index + newSize < MM->size && MM->catalog[index + newSize] > 0)
        newSize += MM->catalog[index + newSize];
    
    // Update the catalog with the results
    MM->catalog[newIndex] = MM->catalog[newIndex + newSize - 1] = newSize;
    
}

// Releases the memory manager initialized to MM
void  mm_release (mm_t *MM) {
    if (MM != NULL) {
        free(MM->start);
        free(MM->catalog);
    }
}
