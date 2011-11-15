#include "packet_public.h"

#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

message_t message;     /* current message structure */
mm_t MM;               /* memory manager will allocate memory for packets */
int pkt_cnt = 0;       /* how many packets have arrived for current message */
int pkt_total = 1;     /* how many packets to be received for the message */
int NumMessages = 5;   /* number of messages we will receive */
int cnt_msg=1;         /*current message being received*/

packet_t get_packet (int size) {
    packet_t pkt;
    static int which;
    
    pkt.how_many = size;
    which=rand()%pkt.how_many; //the order in which packets will be sent is random.
    pkt.which = which;
    if (which == 0)
        strcpy (pkt.data, "aaaaaaa\0");
    else if (which == 1)
        strcpy (pkt.data, "bbbbbbb\0");
    else if(which == 2)
        strcpy (pkt.data, "ccccccc\0");
    else if(which == 3)
        strcpy (pkt.data, "ddddddd\0");
    else
        strcpy (pkt.data, "eeeeeee\0");
    return pkt;
}


void packet_handler(int sig)
{
    packet_t pkt;
    
    
    fprintf (stderr, "IN PACKET HANDLER, sig=%d\n", sig);
    pkt = get_packet(cnt_msg); //the messages are of variable length. So, the 1st message consists of 1 packet, the 2nd message consists of 2 packets and so on..
    if(pkt_cnt==0){ //when the 1st packet arrives, the size of the whole message is allocated.
        message.num_packets = cnt_msg;
        
        for (int i=0; i < cnt_msg; i++)
            message.data[i]= mm_get(&MM, sizeof(data_t));
    } 
    
    pkt_total = pkt.how_many;
    printf("CURRENT MESSAGE %d\n",cnt_msg);
    
    /* insert your code here ... stick packet in memory */
    printf("Which packet: %d\n",pkt.which);
    message.numReceivedOfPacket[pkt.which]++;
    strcpy( (char*) message.data[pkt.which], (const char*) pkt.data);
    pkt_cnt++;
    
    /*Print the packets in the correct order.*/
    if (pkt_cnt == pkt_total)
    {
        printf("Message %d:\n", cnt_msg);
        for (int i = 0; i < pkt_total; i++)
        {
            for (int j = 0; j < message.numReceivedOfPacket[i]; j++)
                printf("%s\n", (char*) message.data[i]);
            mm_put(&MM, message.data[i]);
        }
    }
    
}


int main (int argc, char **argv)
{          
    
    message.num_packets = 0;
    for (int i = 0; i < MaxPackets; i++)
        message.numReceivedOfPacket[i] = 0;
    mm_init (&MM, 200);	
    
  /* set up alarm handler -- mask all signals within it */
    struct sigaction newact;
    newact.sa_handler = packet_handler;
    newact.sa_flags = 0;
    
    sigfillset(&(newact.sa_mask));
    
    sigaction(SIGALRM, &newact, NULL);

  /* turn on alarm timer ... use  INTERVAL and INTERVAL_USEC for sec and usec values */
    
    struct itimerval interval;
    
    interval.it_value.tv_sec = INTERVAL;
    interval.it_value.tv_usec = INTERVAL_USEC;
    interval.it_interval.tv_sec = INTERVAL;
    interval.it_interval.tv_usec = INTERVAL_USEC;
    setitimer(ITIMER_REAL, &interval, NULL);
    
    for (int j=1; j<=NumMessages; j++) {
        while (pkt_cnt < pkt_total) 
            pause();
        
        
        // reset these for next message 
        pkt_total = 1;
        pkt_cnt = 0;
        message.num_packets = 0;
        for (int i = 0; i < MaxPackets; i++)
            message.numReceivedOfPacket[i] = 0;
        
        
        cnt_msg++;
        
    }
    mm_release(&MM);
}

