#include "util-http.h"

#define REQ_BUFFER_LEN 2048

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdio.h>

#include <strings.h>


int global_socket = -1;

void init(int port) {
    
    global_socket = socket (AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET; //IPv4
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind (global_socket, (struct sockaddr*)&addr, sizeof(addr));
    
    //int enable = 1;
    //setsockopt (global_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(int));
    
    listen(global_socket, 5);
    
    printf("init called, global_socket = %d", global_socket);
    
}

int accept_connection(void) {
    
    if (global_socket == -1)
        return -1;
        
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(struct sockaddr);
    
    return accept(global_socket, (struct sockaddr*)&client_addr, &addr_len);  
    
}

int get_request(int fd, char *filename) {
    
    printf ("get_request called");
    
    static char buffer[REQ_BUFFER_LEN];
    char* head = buffer;
    char* tail;
    
    if (global_socket == -1)
        return -1;
    
    
    read(fd, buffer, REQ_BUFFER_LEN);
    
    if (strncasecmp(buffer, "GET ", 4) == 0) {
        head += 4;
        tail = head;
        
        
        int quote = 0;
        int dot = 0;
        int slash = 0;
        
        int i;
        for (i = 0; i < REQ_BUFFER_LEN; ++i) {
            
            
            
            if (*tail == '\\') {
                quote = 1;
                continue;
            } else if (*tail == '.') {
                if (dot) {
                    return -1;
                } else {
                    dot = 1;
                }
            } else if (*tail == '/') {
                if (slash) {
                    return -1;
                } else {
                    slash = 1;
                }
            } else if (*tail == ' ') {
                if (!quote) {
                    break;
                }
            } else {
                quote = 0;
                dot = 0;
                slash = 0;
            }
            
            ++tail;
            
        }
        
        strncpy(filename, head, tail-head);
        filename[tail-head+1] = '\0';
               
        //fprintf(stderr, "Filename: %s", filename);
    }
    
    return 0;
}

int return_result(int fd, char *content_type, char *buf, int numbytes) {
    /* Nothing */
    return 0;
}

int return_error(int fd, char *buf) {
    /* Nothing */
    return 0;
}

int nextguess(char *filename, char *guessed) {
    /* Nothing */
    return 0;
}
