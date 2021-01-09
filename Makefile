UTILS = utils.c
CFLAGS = -Wall -g -lm -lc
CC = gcc

CLIENT_ID = 0
PORT = 2222
IP_SERVER = 127.0.0.1

all: build

build: server subscriber

server: server.c
	${CC} ${CFLAGS} $^ ${UTILS} -o server

subscriber: subscriber.c
	${CC} ${CFLAGS} $^ ${UTILS} -o subscriber

run_server:
	./server ${PORT}

run_subscriber:
	./subscriber ${CLIENT_ID} ${IP_SERVER} ${PORT}

.PHONY: clean server subscriber
clean:
	rm -rf server subscriber unsent
