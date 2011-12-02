CC = gcc
CFLAGS = -D_REENTRANT
LDFLAGS = -lpthread

all: web_server_http_mac

web_server_http: server.o util-http.o
	${CC} ${LDFLAGS} -o web_server_http server.o util-http.o

web_server_http_mac: server.o util-http_MAC.o
	${CC} ${LDFLAGS} -o web_server_http_mac server.o util-http_MAC.o

server.o: server.c

clean:
	rm server.o web_server_http web_werver_http_mac

