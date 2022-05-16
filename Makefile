CC=g++
AR=ar
FLAGS= -Wall -g

all: client server

client: client.o StackOverLinkedList.h
	$(CC) $(FLAGS) -o client client.o

server:  server.o StackOverLinkedList.h
	$(CC) $(FLAGS) -o server server.o

server.o: server.cpp
	$(CC) $(FLAGS) -c server.cpp

client.o: client.cpp
	$(CC) $(FLAGS) -c client.cpp

.PHONY: clean all
clean:
	rm -f *.o client server