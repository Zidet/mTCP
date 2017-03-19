CC=gcc
CFLAGS=-Wall -I. -g
LDFLAGS=-lpthread -lrt

EXE = server

OBJ = server.o mtcp_server.o mtcp_common.o

${EXE}: ${OBJ}
	${CC} ${CFLAGS} -o ${EXE} ${OBJ} ${LDFLAGS}

clean:
	rm -f ${EXE} ${OBJ}
