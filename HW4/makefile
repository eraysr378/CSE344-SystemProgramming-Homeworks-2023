
all: server.o client.o
	gcc server.o -o server -lpthread -lrt
	gcc client.o -o client -lpthread -lrt

server: server.o
	gcc server.o -o server -lpthread -lrt

server.o: server.c client.o functions.h myconst.h mytypes.h pidqueue.h taskqueue.h
	gcc -c server.c 

client: client.o
	gcc client.o -o client -lpthread -lrt

cleint.o: client.c client.o functions.h myconst.h mytypes.h pidqueue.h taskqueue.h
	gcc -c client.c

clean:
	rm server *.o client
	