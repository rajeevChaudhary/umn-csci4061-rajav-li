#include "mm_public.h"

#define MaxPackets 10

typedef char data_t[8];

struct packet_t {
  int how_many; /* number of packets in the message */
  int which;    /* which packet in the message -- currently ignored */
  data_t data;  /* packet data */
};


/* Keeps track of packets that have arrived for the message */
struct message_t {
  int num_packets;
  void *data[MaxPackets];
};

