#include "mm_public.h"

int main (int argc, char **argv)
{
  int j, i;
  struct timeval times, timee;
 void *chunk = NULL;
  mm_t MM;

  j = gettimeofday (&times, NULL);
  // I modified the mm_init call to be scalable
  if (mm_init(&MM, how + 1) < 0)
    perror("mm_init");
for (i=0; i< how; i++) { 
    chunk = mm_get(&MM, i+1);
    mm_put(&MM,chunk);
}
mm_release(&MM);
j = gettimeofday (&timee, NULL);
fprintf (stderr, "MM time took %f msec\n",comp_time (times, timee)/1000.0);
}
