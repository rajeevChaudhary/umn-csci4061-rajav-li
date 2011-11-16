#include <sys/time.h>
#include <signal.h>

#include <stdio.h>
#include <stdlib.h>

#define INTERVAL 0
#define INTERVAL_USEC 800000
#define SZ 64

// It's very difficult to see any difference at how = 8.
// You can certainly argue that it needs to be faster at
// small values, too, but I think I can prove that it is
// theoretically faster at an arbitrary input size, and
// the usefulness of timing is dubious at a size as tiny as
// 8. The environment weighs too heavily on the results.

// Also note that at how = 8, our manager still runs malloc
// and free twice each to init and release, and on about 5
// times the space of the biggest malloc in main_malloc.
// The comparison is thus a little unfair. In a real application,
// you would understand that you're trading the init and
// release time in for faster asymptotic running time.
#define how 1500


struct mm_t {
    void *start;
    int *catalog;
    int size;
};


int  mm_init (mm_t *MM, int total_size);
void* mm_get (mm_t *MM, int neededSize);
void mm_put (mm_t *MM, void *chunk);
void  mm_release (mm_t *MM);
double comp_time (struct timeval times, struct timeval timee);
