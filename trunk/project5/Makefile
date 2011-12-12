CC = gcc
FLAGS = -Wall

all: web_server_http

web_server_http: sol_server.c util-http.o
	$(CC) $(FLAGS) -o web_server_http sol_server.c util-http.o

web_server_file: sol_server.c util-http-file.o
	$(CC) $(FLAGS) -o web_server_file sol_server.c util-http-file.o

util-http.o: util-http.c util-http.h
	$(CC) $(FLAGS) -c util-http.c

util-http-file.o: util-http-file.c util-http-file.h
	$(CC) $(FLAGS) -c util-http-file.c

clean:
	rm *.o web_server_http
