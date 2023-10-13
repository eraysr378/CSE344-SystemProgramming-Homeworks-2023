
all: main.o
	gcc main.o -o main -lpthread -lrt
	
main: main.o
	gcc main.o -o main -lpthread -lrt

main.o: main.c mytypes.h myfunctions.h myglobals.h
	gcc -c main.c 

clean:
	rm main *.o
	