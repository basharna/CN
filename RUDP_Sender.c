#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <stdbool.h>
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

// RUDP header
typedef struct {
    uint16_t length;
    uint16_t checksum;
    uint8_t flags;
} RUDP_Header;

// rudp packet
typedef struct {
    RUDP_Header header;
    char data[BUFFER_SIZE];
} RUDP_Packet;

int main(int argc, char *argv[]) {

    char *server_ip;
    int server_port;


    if(argc != 5){
        fprintf(stderr, "Usage: %s -ip <server_ip> -p <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-ip") == 0)
        {
            server_ip = argv[i+1];
        }
        else if (strcmp(argv[i], "-p") == 0)
        {
            server_port = atoi(argv[i+1]);
        }
    }
    

    fprintf(stdout, "Starting Sender...\n");

    // buffer to store the file
    char *file_data = (char *)calloc(BUFFER_SIZE, sizeof(char));

    // Open the file 
    FILE *file = fopen("data.txt", "r");
    if (file == NULL) {
        perror("fopen(3)");
        free(file_data);
        exit(EXIT_FAILURE);
    }

    // read the file into a buffer
    int bytes_read;
    if ((bytes_read = fread(file_data, sizeof(char), BUFFER_SIZE, file)) <= BUFFER_SIZE) {
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
    if (rudp_connect(sock, server_ip, server_port) == 0) {
        fprintf(stderr, "Failed to connect to the receiver.\n");
        rudp_close(sock);
        free(file_data);
        exit(EXIT_FAILURE);
    }

    // create header with SYN flag
    RUDP_Header header;
    header.flags = 1; // SYN flag
    header.length = 0;
    header.checksum = 0;

    // send the SYN header
    int sent_bytes = rudp_send(sock, (char *)&header, sizeof(header));
    if (sent_bytes < 0) {
        fprintf(stderr, "Failed to send the SYN header.\n");
        rudp_close(sock);
        free(file_data);
        exit(EXIT_FAILURE);
    }

    // receive the SYN-ACK header
    RUDP_Header syn_ack_header;
    int received_bytes = rudp_recv(sock, (char *)&syn_ack_header, sizeof(syn_ack_header));
    if (received_bytes < 0) {
        fprintf(stderr, "Failed to receive the SYN-ACK header.\n");
        rudp_close(sock);
        free(file_data);
        exit(EXIT_FAILURE);
    }

    // check if the received header is a SYN-ACK
    if (syn_ack_header.flags != 3) { // SYN-ACK flag
        fprintf(stderr, "Received invalid SYN-ACK header.\n");
        rudp_close(sock);
        free(file_data);
        exit(EXIT_FAILURE);
    }

    // send the ACK header
    header.flags = 2; // ACK flag
    sent_bytes = rudp_send(sock, (char *)&header, sizeof(header));
    if (sent_bytes < 0) {
        fprintf(stderr, "Failed to send the ACK header.\n");
        rudp_close(sock);
        free(file_data);
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, "Connected to the receiver.\n");

    char decision;
    do
    {

        // create a packet
        RUDP_Packet packet;
        packet.header.length = bytes_read;
        packet.header.checksum = 0; // initialize checksum
        //set flags to PUSH ACK
        packet.header.flags = 18; // PUSH-ACK flag

        // copy file data to packet
        memcpy(packet.data, file_data, bytes_read);

        // calculate checksum
        uint16_t checksum = calculate_checksum(file_data, bytes_read);
        packet.header.checksum = checksum;

        // send the packet
        sent_bytes = rudp_send(sock, (char *)&packet, sizeof(packet));
        if (sent_bytes < 0) {
            fprintf(stderr, "Failed to send the packet.\n");
            rudp_close(sock);
            free(file_data);
            exit(EXIT_FAILURE);
        }

        fprintf(stdout, "File sent.\n");

        // receive response packet from the receiver
        RUDP_Packet rec_packet;
        received_bytes = rudp_recv(sock, (char *)&rec_packet, sizeof(rec_packet));
        if (received_bytes < 0) {
            fprintf(stderr, "Failed to receive the response packet.\n");
            rudp_close(sock);
            free(file_data);
            exit(EXIT_FAILURE);
        }
        // if the received packet is a FIN ACK, send ACK and close the connection
        if (rec_packet.header.flags == 5) { // FIN-ACK flag
            header.flags = 2; // ACK flag
            sent_bytes = rudp_send(sock, (char *)&header, sizeof(header));
            if (sent_bytes < 0) {
                fprintf(stderr, "Failed to send the ACK header.\n");
                rudp_close(sock);
                free(file_data);
                exit(EXIT_FAILURE);
            }
            break;
        }
        scanf(" %c", &decision);
    } while (decision == 'Y' || decision == 'y');
    

    // send FIN ACK packet
    header.flags = 5; // FIN-ACK flag
    sent_bytes = rudp_send(sock, (char *)&header, sizeof(header));
    if (sent_bytes < 0) {
        fprintf(stderr, "Failed to send the FIN-ACK header.\n");
        rudp_close(sock);
        free(file_data);
        exit(EXIT_FAILURE);
    }

    // disconnect from the receiver and close the socket
    rudp_disconnect(sock);
    rudp_close(sock);
    free(file_data);

}