CC = gcc
FLAGS = -Wall -g

all: web_server_http

web_server_http: sol_server.c util-http.o
	$(CC) $(FLAGS) -o web_server_http sol_server.c util-http.o

util-http.o: util-http.c util-http.h
	$(CC) $(FLAGS) -c util-http.c

clean:
	rm *.o web_server_http
