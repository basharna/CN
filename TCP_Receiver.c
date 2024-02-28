#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#define SERVER_IP "127.0.0.1"
#define MAX_CLIENTS 1
#define BUFFER_SIZE 2 * 1024 * 1024

typedef struct {
    double time_taken;
    double bandwidth;
} FileStats;

int main(int argc, char *argv[]) {

    int server_port;
    char *algorithm;

    if(argc != 5){
        fprintf(stderr, "Usage: %s -p <server_port> -algo <algorithm>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0)
        {
            server_port = atoi(argv[i+1]);
        }
        else if (strcmp(argv[i], "-algo") == 0)
        {
            algorithm = argv[i+1];
        }
    }


    fprintf(stdout, "Starting Receiver...\n");

    // The variable to store the socket file descriptor.
    int sock = -1;

    // The variable to store the receiver's and sender's address.
    struct sockaddr_in sender_addr, receiver_addr;

    // Stores the sender's structure length.
    socklen_t sender_addr_len = sizeof(sender_addr);

    // Reset the receiver and sender structures to zeros.
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    memset(&sender_addr, 0, sizeof(sender_addr));

    // Try to create a TCP socket (IPv4).
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1){
        perror("socket(2)");
        exit(EXIT_FAILURE);
    }

    int optval = 1;
    // Set the socket option to reuse the server's address.
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0){
        perror("setsockopt(2)");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Set the congestion control algorithm.
    if(setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, algorithm, strlen(algorithm)) < 0){
        perror("setsockopt(2)");
        close(sock);
        exit(EXIT_FAILURE);
    }

     // Set the receiver's address family to AF_INET (IPv4).
    receiver_addr.sin_family = AF_INET;
    // Set the receiver's address.
    if (inet_pton(AF_INET, SERVER_IP, &receiver_addr.sin_addr) <= 0) {
        perror("inet_pton(3)");
        close(sock);
        exit(EXIT_FAILURE);
    }
    // Set the receiver's port number.
    receiver_addr.sin_port = htons(server_port);

    // Bind the socket to the receiver's address.
    if (bind(sock, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr)) < 0){
        perror("bind(2)");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections.
    if (listen(sock, MAX_CLIENTS) < 0){
        perror("listen(2)");
        close(sock);
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "Waiting for TCP connection...\n");
    
    //accept connection from an incoming client
    int sender_sock = accept(sock, (struct sockaddr *)&sender_addr, &sender_addr_len);
    if (sender_sock < 0){
        perror("accept(2)");
        close(sock);
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "Connection accepted from %s:%d\n", inet_ntoa(sender_addr.sin_addr), ntohs(sender_addr.sin_port));

    char received_data[BUFFER_SIZE];
    FileStats *fileStats = NULL;
    int fileStatsCount = 0;
    double total_time_taken = 0;
    double total_bandwidth = 0;

    while(1){
        
        clock_t start, end;
        int total_bytes_received = 0;

        //start the timer
        start = clock();
        do{
            // Receive the file
            int bytes_received = recv(sender_sock, received_data, BUFFER_SIZE, 0);
            if (bytes_received < 0){
                perror("recv(2)");
                close(sender_sock);
                close(sock);
                free(fileStats);
                exit(EXIT_FAILURE);
            }else if (bytes_received == 0){
                fprintf(stdout, "Connection closed by sender.\n");
                close(sender_sock);
                close(sock);
                break;
            }
            total_bytes_received += bytes_received;

            // Check if received_data contains the end-of-file marker
            if (total_bytes_received == BUFFER_SIZE){
                // Stop the clock
                end = clock();

                //measure the time in milliseconds taken to receive the file
                double time_taken = ((double)(end - start)) / (CLOCKS_PER_SEC / 1000);
                total_time_taken += time_taken;

                //calculate the bandwidth in MB/s
                double bandwidth = (BUFFER_SIZE / (time_taken / 1000)) / (1024 * 1024);
                total_bandwidth += bandwidth;

                fileStatsCount++;
                fileStats = realloc(fileStats, fileStatsCount * sizeof(FileStats));
                if (fileStats == NULL) {
                    perror("realloc(3)");
                    close(sender_sock);
                    close(sock);
                    free(fileStats);
                    exit(EXIT_FAILURE);
                }
                //store the file statistics
                fileStats[fileStatsCount - 1].time_taken = time_taken;
                fileStats[fileStatsCount - 1].bandwidth = bandwidth;

                // Send acknowledgment back to the sender
                send(sender_sock, "ACK", 3, 0);

                break; 
            }

        } while (1);

        if(strcmp(received_data, "exit") == 0){
            fprintf(stdout, "Sender sent exit message.\n");
            close(sender_sock);
            close(sock);
            break;
        }

        fprintf(stdout, "File received. Bytes received: %d\n", total_bytes_received);
        
        fprintf(stdout, "Waiting for Sender response...\n");
    }
    
    // Print the file statistics
    fprintf(stdout, "-----------------------\n");
    fprintf(stdout, "File Statistics:\n");
    for (size_t i = 0; i < fileStatsCount; i++)
    {
        double bandwidth = fileStats[i].bandwidth;
        double time = fileStats[i].time_taken;
        fprintf(stdout, "Run %zu: Time = %.2f ms, Speed = %.2f MB/s\n", i + 1, time, bandwidth);
    }

        // Print the average file statistics
        fprintf(stdout, "Average time: %.2f ms\n", total_time_taken / fileStatsCount);
        fprintf(stdout, "Average bandwidth: %.2f MB/s\n", total_bandwidth / fileStatsCount);
        
        fprintf(stdout, "-----------------------\n");
        

    
    fprintf(stdout, "Receiver end\n");
    free(fileStats);
    return 0;
}