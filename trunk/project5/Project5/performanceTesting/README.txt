Use the html folder as your web root directory.
Use a cache size of 10 for your performance evaluation (when the cache is
enabled).
Use a request queue size of 10.
Use requestlist.txt for the list of requests, e.g.:
  ./web_server_file [arguments...] < requestlist.txt

For each setup configuration, run the test a few times (for "warmup"), then 
to make your measurements, run the program 3 more times and take the average 
of the last 3 trials.

If you run into a situation where the times are like: 100ms, 120ms, 900ms,
then you probably need to re-run the last measurement. That is, if it's an
obvious outlier, do not let it skew your results. The point is to produce
stable results, and take an average of 3 stable results. Since we are working
in a shared environment and over NFS, these outlier results may happen. We
wanted to be clear how you should deal with these.