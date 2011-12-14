CC = gcc
CFLAGS = -Wall -D_REENTRANT
LDFLAGS = -lpthread

all: web_server_http

web_server_http: server.c util-http.o guess.o
	${CC} ${LDFLAGS} -o web_server_http server.c util-http.o guess.o

web_server_solution: sol_server.c util-http.o guess.o
	${CC} ${LDFLAGS} -o web_server_solution sol_server.c util-http.o guess.o

web_server_file: server.c util-http-file.o guess.o
	${CC} ${LDFLAGS} -o web_server_file server.c util-http-file.o guess.o

web_server_file_solution: sol_server.c util-http-file.o guess.o
	${CC} ${LDFLAGS} -o web_server_solution_file sol_server.c util-http-file.o guess.o

util-http.o: util-http.c util-http.h
	${CC} -c util-http.c

util-http-file.o: util-http-file.c util-http-file.h
	${CC} -c util-http-file.c

clean:
	rm -f *.o web_server_http web_server_solution web_server_file web_server_solution_file
