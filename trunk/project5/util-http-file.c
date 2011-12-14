#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define REQ_BUFFER_LEN 2048

void init(int port) {
    /* No-op */
}

int accept_connection(void) {
    
    if (feof(stdin) != 0 || ferror(stdin) != 0)
        return -1;
    else
        return 0;
    
}

int get_request(int fd, char *filename) {
    
    if (fgets(filename, 1024, stdin) == NULL)
        return -1;
    else
        return 0;
    
}

int return_result(int fd, char *content_type, char *buf, int numbytes) {
    
    /* No-op */
    
    return 0;
}

int return_error(int fd, char *buf) {
    
    /* No-op */
    
    return 0;
}
