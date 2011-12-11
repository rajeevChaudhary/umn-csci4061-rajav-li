#include "util-http.h"



void init(int port) {
    //Fill in
}

int accept_connection(void) {
    //Fill in
}

int get_request(int fd, char *filename) {
    //Fill in
}

int return_result(int fd, char *content_type, char *buf, int numbytes) { /* Nothing */ }

int return_error(int fd, char *buf) { /* Nothing */ }

int nextguess(char *filename, char *guessed) { /* Nothing */ }
