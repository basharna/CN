CC = gcc
CFLAGS = -Wall


all : TCP_Receiver TCP_Sender file_generator RUDP_Receiver RUDP_Sender

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

TCP_Receiver: TCP_Receiver.o
	$(CC) $(CFLAGS) -o $@ $^

TCP_Sender: TCP_Sender.o
	$(CC) $(CFLAGS) -o $@ $^

file_generator: file_generator.o
	$(CC) $(CFLAGS) -o $@ $^

RUDP_Receiver: RUDP_Receiver.o
	$(CC) $(CFLAGS) -o $@ $^

RUDP_Sender: RUDP_Sender.o
	$(CC) $(CFLAGS) -o $@ $^

RUDP_Sender.o: RUDP_Sender.c RUDP_API.c
	$(CC) $(CFLAGS) -c $< -o $@

RUDP_Receiver.o: RUDP_Receiver.c RUDP_API.c
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -f *.o TCP_Receiver TCP_Sender file_generator RUDP_Receiver RUDP_Sender
