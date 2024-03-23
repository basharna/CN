#include "RUDP_API.c"

#define BUFFER_SIZE 2 * 1024 * 1024

/*
0
0
0
0 PUSH
0
0
0 ACK
0 SYN
*/

int main(int argc, char *argv[])
{

    char *server_ip;
    int server_port;

    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s -ip <server_ip> -p <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-ip") == 0)
        {
            server_ip = argv[i + 1];
        }
        else if (strcmp(argv[i], "-p") == 0)
        {
            server_port = atoi(argv[i + 1]);
        }
    }

    fprintf(stdout, "Starting Sender...\n");

    // buffer to store the file
    char *file_data = (char *)calloc(BUFFER_SIZE, sizeof(char));

    // Open the file
    FILE *file = fopen("data.txt", "r");
    if (file == NULL)
    {
        perror("fopen(3)");
        free(file_data);
        exit(EXIT_FAILURE);
    }

    // read the file into a buffer
    int bytes_read;
    if ((bytes_read = fread(file_data, sizeof(char), BUFFER_SIZE, file)) < BUFFER_SIZE)
    {
        perror("fread(3)");
        fclose(file);
        free(file_data);
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "bytes read: %d.\n", bytes_read);

    // Close the file
    fclose(file);

    // Create a UDP socket between the Sender and the Receiver.
    RUDP_Socket *sock = rudp_socket(false, server_port);

    fprintf(stdout, "Socket created.\n");

    // Connect to the receiver
    if (rudp_connect(sock, server_ip, server_port) == 0)
    {
        fprintf(stderr, "Failed to connect to the receiver.\n");
        rudp_close(sock);
        free(file_data);
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, "Connected to the receiver.\n");


    RUDP_Packet rec_packet;

    char decision;
    do
    {
        // create a packet
        RUDP_Packet packet;
        packet.header.length = bytes_read;
        packet.header.checksum = 0; // initialize checksum
        // set flags to PUSH ACK
        packet.header.flags = 18; // PUSH-ACK flag

        // copy file data to packet
        memcpy(packet.data, file_data, bytes_read);

        // calculate checksum
        uint16_t checksum = calculate_checksum(file_data, bytes_read);
        packet.header.checksum = checksum;

        // send the packet
        int sent_bytes = rudp_send(sock, &packet, sizeof(packet));
        if (sent_bytes < 0)
        {
            fprintf(stderr, "Failed to send the packet.\n");
            rudp_close(sock);
            free(file_data);
            exit(EXIT_FAILURE);
        }

        fprintf(stdout, "File sent.\n");

        // receive response packet from the receiver
        int received_bytes = rudp_recv(sock, &rec_packet, sizeof(rec_packet));
        if (received_bytes < 0)
        {
            fprintf(stderr, "Failed to receive the response packet.\n");
            rudp_close(sock);
            free(file_data);
            exit(EXIT_FAILURE);
        }

        scanf(" %c", &decision);
    } while (decision == 'Y' || decision == 'y');

    // disconnect from the receiver and close the socket
    rudp_disconnect(sock);
    free(file_data);
    return 0;
}