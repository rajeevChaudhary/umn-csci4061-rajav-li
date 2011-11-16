#include "mm_public.h"

int main (int argc, char **argv)
{
  int j, i;
  struct timeval times, timee;
  // I don't understand why this pointer had to be an array.
  // As an array it enforces a needless hard limit.
  // I made it a single void pointer instead and modified
  // the for loop below to match.
  void *b;
  struct itimerval interval;
  struct itimerval oldinterval;


  j = gettimeofday (&times, (void *)NULL);
  for (i=0; i<how; i++) {
    b = (void*)malloc (i+1);
    free (b);
}
  j = gettimeofday (&timee, (void *)NULL);
  fprintf (stderr, "MALLOC/FREE time took %f msec\n",
          comp_time (times, timee)/1000.0);

}


