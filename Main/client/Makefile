CC=gcc
CFLAGS=-Wall -I. -g
LDFLAGS=-lpthread -lrt

EXE = client

OBJ = client.o mtcp_client.o circular_queue.o mtcp_common.o

${EXE}: ${OBJ}
	${CC} ${CFLAGS} -o ${EXE} ${OBJ} ${LDFLAGS}

clean:
	rm -f ${EXE} ${OBJ}
