CC = gcc
CFLAGS = -Wall

SRCS = TCP_Receiver.c TCP_Sender.c

all : TCP_Receiver TCP_Sender file_generator

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

TCP_Receiver: TCP_Receiver.o
	$(CC) $(CFLAGS) -o $@ $^

TCP_Sender: TCP_Sender.o
	$(CC) $(CFLAGS) -o $@ $^

file_generator: file_generator.o
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o TCP_Receiver TCP_Sender file_generator
