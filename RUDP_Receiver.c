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

    RUDP_Header header;
    header.length = 0;
    header.checksum = 0;
    header.flags = 0;
    RUDP_Packet packet;
    packet.header = header;

    // int recv = rudp_recv(sock, &packet, sizeof(packet));
    // if (recv == -1)
    // {
    //     perror("rudp_recv(3)");
    //     exit(EXIT_FAILURE);
    // }

    // // if received syn send back syn ack
    // if (packet.header.flags == 1)
    // {
    //     packet.header.flags = 3;
    //     if (rudp_send(sock, &packet, sizeof(packet)) == -1)
    //     {
    //         perror("rudp_send(3)");
    //         exit(EXIT_FAILURE);
    //     }
    // }

    // // receive ack
    // recv = rudp_recv(sock, &packet, sizeof(packet));
    // if (recv == -1)
    // {
    //     perror("rudp_recv(3)");
    //     exit(EXIT_FAILURE);
    // }

    FileStats *fileStats = NULL;
    int fileStatsCount = 0;
    double total_time_taken = 0;
    double total_bandwidth = 0;

    while (1)
    {
        clock_t start, end;
        int total_bytes_received = 0;
        // start clock
        start = clock();
        while (1)
        {
            int recv_len = rudp_recv(sock, &packet, sizeof(packet));
            if (recv_len == 0)
            {
                fprintf(stdout, "Received FIN packet. Exiting...\n");
                break;
            }
            else if (recv_len == -1)
            {
                perror("rudp_recv(3)");
                exit(EXIT_FAILURE);
            }
            total_bytes_received += packet.header.length;

            fprintf(stdout, "Received %d bytes\n", recv_len);

            // send ack
            packet.header.flags = 2;
            if (rudp_send(sock, &packet, sizeof(packet)) == -1)
            {
                perror("rudp_send(3)");
                exit(EXIT_FAILURE);
            }
            // Check if received_data contains the end-of-file marker
            if (total_bytes_received == BUFFER_SIZE)
            {
                // Stop the clock
                end = clock();

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
                break;
            }
        }

        // if received fin ack packet break
        if (packet.header.flags == 5)
        {
            fprintf(stdout, "Sender sent FIN message.\n");
            rudp_disconnect(sock);
            rudp_close(sock);
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