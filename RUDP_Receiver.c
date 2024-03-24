#include "RUDP_API.c"

#define SERVER_IP "127.0.0.1"

#define BUFFER_SIZE 2 * 1024 * 1024

typedef struct
{
    double time_taken;
    double bandwidth;
} FileStats;

int main(int argc, char *argv[])
{

    int server_port;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s -p <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0)
        {
            server_port = atoi(argv[i + 1]);
        }
    }

    fprintf(stdout, "Starting Receiver...\n");

    // Create a UDP socket between the Sender and the Receiver.
    RUDP_Socket *sock = rudp_socket(true, server_port);

    if (rudp_accept(sock) == 0)
    {
        perror("rudp_accept(3)");
        exit(EXIT_FAILURE);
    }

    FileStats *fileStats = NULL;
    int fileStatsCount = 0;
    double total_time_taken = 0;
    double total_bandwidth = 0;

    RUDP_Packet packet;
    while (1)
    {
        clock_t start, end;
        
        // start clock
        start = clock();
        int recv_len = rudp_receive(sock, &packet);
        if (recv_len == 0)
        {
            printf("Received FIN packet. Exiting...\n");
            break;
        }
        else if (recv_len == -1)
        {
            perror("rudp_recv(3)");
            exit(EXIT_FAILURE);
        }
        end = clock();

        // send ack
        int sent_len = rudp_send(sock, ACK, NULL, 0);
        if (sent_len == -1)
        {
            perror("rudp_send(3)");
            exit(EXIT_FAILURE);
        }

        // measure the time in milliseconds taken to receive the file
        double time_taken = ((double)(end - start)) / (CLOCKS_PER_SEC / 1000);
        total_time_taken += time_taken;

        // calculate the bandwidth in MB/s
        double bandwidth = (BUFFER_SIZE / (time_taken / 1000)) / (1024 * 1024);
        total_bandwidth += bandwidth;

        fileStatsCount++;
        fileStats = realloc(fileStats, fileStatsCount * sizeof(FileStats));
        if (fileStats == NULL)
        {
            perror("realloc(3)");
            rudp_disconnect(sock);
            rudp_close(sock);
            free(fileStats);
            exit(EXIT_FAILURE);
        }
        // store the file statistics
        fileStats[fileStatsCount - 1].time_taken = time_taken;
        fileStats[fileStatsCount - 1].bandwidth = bandwidth;

        fprintf(stdout, "File received. Bytes received: %d\n", recv_len);

        fprintf(stdout, "Waiting for Sender response...\n");
    }

    // send FIN-ACK
    printf("Sending FIN-ACK...\n");
    if (rudp_send(sock, FIN_ACK, NULL, 0) == -1)
    {
        perror("rudp_send(3)");
        exit(EXIT_FAILURE);
    }

    // receive ACK
    if (rudp_receive(sock, &packet) == -1)
    {
        perror("rudp_recv(3)");
        exit(EXIT_FAILURE);
    }
    printf("Received ACK. Closing connection...\n");

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

    rudp_close(sock);
    return 0;
}