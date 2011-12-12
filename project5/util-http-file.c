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
#include <stdlib.h>

#include <strings.h>
#include <pthread.h>

pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;


int global_socket = -1;

void init(int port) {
    /*
    global_socket = socket (AF_INET, SOCK_STREAM, 0);
    
    if ( global_socket < 0 )
        exit(1);
    
    struct sockaddr_in addr;
    
    bzero(&addr, sizeof(struct sockaddr_in));
    
    addr.sin_family = AF_INET; //IPv4
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if ( bind(global_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0 )
        exit(1);
    
    int enable = 1;
    setsockopt (global_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(int));
    
    if ( listen(global_socket, 5) < 0 )
        exit(1);
    
    printf("init called, global_socket = %d\n", global_socket);
    */
}

int accept_connection(void) {
    /*
    pthread_mutex_lock(&accept_mutex);
    
    if (global_socket == -1)
        return -1;
        
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(struct sockaddr);
    
    int ret = accept(global_socket, (struct sockaddr*)&client_addr, &addr_len);  
    
    pthread_mutex_unlock(&accept_mutex);
    
    return ret;
     */
    
    if (feof(stdin) != 0 || ferror(stdin) != 0)
        return -1;
    else
        return 0;
}

int get_request(int fd, char *filename) {
    
    /*
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
    }
    
    return 0;
     */
    
    if (fgets(filename, 1024, stdin) == NULL)
        return -1;
    else
        return 0;
}

int return_result(int fd, char *content_type, char *buf, int numbytes) {
    
    /*
    FILE* stream = fdopen(fd, "w");
    
    fprintf(stderr, "HTTP/1.1 200 OK\nContent-Type: %s\nContent-Length: %d\nConnection: Close\n\n", content_type, numbytes);
    fprintf(stream, "HTTP/1.1 200 OK\nContent-Type: %s\nContent-Length: %d\nConnection: Close\n\n", content_type, numbytes);
    fflush(stream);
    
    if (write(fd, buf, numbytes) < 0)
        fprintf(stderr, "Write to socket failed\n");
    
    fclose(stream);
    */
    return 0;
}

int return_error(int fd, char *buf) {
    /*
    FILE* stream = fdopen(fd, "w");
    
    fprintf(stream, "HTTP/1.1 401 Not Found\nContent-Type: text/html\nContent-Length: %zu\nConnection: Close\n\n", strlen(buf));
    fflush(stream);
    
    if (write(fd, buf, strlen(buf)) < 0)
        fprintf(stderr, "Write to socket failed\n");
    
    fclose(stream);
    */
    return 0;
}

int nextguess(char *filename, char *guessed) {
    /* Nothing */
    return 0;
}
