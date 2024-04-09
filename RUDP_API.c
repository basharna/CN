#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <sys/time.h>

#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 2 * 1024 * 1024
#define MAX_WAIT_TIME 2
#define CHUNK_SIZE 1024
#define SYN 1
#define SYN_ACK 3
#define ACK 2
#define FIN 4
#define FIN_ACK 6
#define PUSH 16

/*
0
0
0
0 PUSH
0
0 FIN
0 ACK
0 SYN
*/

unsigned short int calculate_checksum(void *data, unsigned int bytes)
{
    unsigned short int *data_pointer = (unsigned short int *)data;
    unsigned int total_sum = 0;
    // Main summing loop
    while (bytes > 1)
    {
        total_sum += *data_pointer++;
        bytes -= 2;
    }
    // Add left-over byte, if any
    if (bytes > 0)
        total_sum += *((unsigned char *)data_pointer);
    // Fold 32-bit sum to 16 bits
    while (total_sum >> 16)
        total_sum = (total_sum & 0xFFFF) + (total_sum >> 16);
    return (~((unsigned short int)total_sum));
}

// RUDP header
typedef struct
{
    uint32_t checksum;
    uint16_t length;
    uint16_t sequence_number;
    uint16_t acknowledgment_number;
    uint8_t flags;
} RUDP_Header;

// rudp packet
typedef struct
{
    RUDP_Header header;
    char data[CHUNK_SIZE];
} RUDP_Packet;

// A struct that represents RUDP Socket
typedef struct
{
    int socket_fd;                // UDP socket file descriptor
    bool isServer;                // True if the RUDP socket acts like a server, false for client.
    bool isConnected;             // True if there is an active connection, false otherwise.
    struct sockaddr_in dest_addr; // Destination address. Client fills it when it connects via rudp_connect(), server fills it when it accepts a connection via rudp_accept().
} RUDP_Socket;

int rudp_close(RUDP_Socket *);
int rudp_send(RUDP_Socket *, uint8_t, char *, size_t);
int rudp_receive(RUDP_Socket *, RUDP_Packet *);

// Allocates a new structure for the RUDP socket (contains basic information about the socket itself).
// Also creates a UDP socket as a baseline for the RUDP.
// isServer means that this socket acts like a server. If set to server socket, it also binds the socket to a specific port.
RUDP_Socket *rudp_socket(bool isServer, unsigned short int listen_port)
{
    RUDP_Socket *sockfd = (RUDP_Socket *)malloc(sizeof(RUDP_Socket));
    if (sockfd == NULL)
    {
        perror("malloc(3)");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server;

    // Reset the receiver to zeros.
    memset(&server, 0, sizeof(server));

    // Set the receiver's address.
    if (inet_pton(AF_INET, SERVER_IP, &server.sin_addr) <= 0)
    {
        perror("inet_pton(3)");
        exit(EXIT_FAILURE);
    }

    // Set the server's address family to AF_INET (IPv4).
    server.sin_family = AF_INET;

    // Set the server's port to the specified port. Note that the port must be in network byte order.
    server.sin_port = htons(listen_port);

    sockfd->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd->socket_fd < 0)
    {
        perror("socket(2)");
        free(sockfd);
        exit(EXIT_FAILURE);
    }

    sockfd->isServer = isServer;
    sockfd->isConnected = false;

    if (isServer)
    {

        if (bind(sockfd->socket_fd, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
            perror("bind(2)");
            close(sockfd->socket_fd);
            free(sockfd);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        struct timeval tv = {MAX_WAIT_TIME, 0};
        if (setsockopt(sockfd->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv)) == -1)
        {
            perror("setsockopt(2)");
            close(sockfd->socket_fd);
            exit(EXIT_FAILURE);
        }
    }

    return sockfd;
}

// Tries to connect to the other side via RUDP to given IP and port.
// Returns 0 on failure and 1 on success.
// Fails if called when the socket is connected/set to server.
int rudp_connect(RUDP_Socket *sockfd, const char *dest_ip, unsigned short int dest_port)
{
    if (sockfd->isServer || sockfd->isConnected)
    {
        fprintf(stderr, "Socket is already connected.\n");
        return 0;
    }
    memset(&sockfd->dest_addr, 0, sizeof(sockfd->dest_addr));
    sockfd->dest_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, dest_ip, &sockfd->dest_addr.sin_addr) <= 0)
    {
        perror("inet_pton(3)");
        return 0;
    }
    sockfd->dest_addr.sin_port = htons(dest_port);

    // send syn
    printf("Sending SYN packet.\n");
    int sent = rudp_send(sockfd, SYN, NULL, 0);
    if (sent == -1)
    {
        printf("Failed to send SYN packet.\n");
        return 0;
    }

    RUDP_Packet packet;
    int recv = rudp_receive(sockfd, &packet);
    if (recv == -1)
    {
        printf("Failed to receive SYN-ACK packet.\n");
        return 0;
    }
    if (packet.header.flags == SYN_ACK)
    {
        printf("Received SYN-ACK packet.\n");
        // send ack
        printf("Sending ACK packet.\n");
        sent = rudp_send(sockfd, ACK, NULL, 0);
        if (sent == -1)
        {
            return 0;
        }
        sockfd->isConnected = true;
    }
    else
    {
        printf("Received unexpected packet.\n");
        printf("Received %d\n", packet.header.flags);
        return 0;
    }
    return 1;
}

// Accepts incoming connection request and completes the handshake, returns 0 on failure and 1 on success.
// Fails if called when the socket is connected/set to client.
int rudp_accept(RUDP_Socket *sockfd)
{
    if (!sockfd->isServer || sockfd->isConnected)
    {
        return 0;
    }

    printf("Waiting for connection...\n");

    RUDP_Packet packet;
    int recv = rudp_receive(sockfd, &packet);
    if (recv == -1)
    {
        return 0;
    }

    //  if received syn send back syn ack
    if (packet.header.flags == SYN)
    {
        printf("Received SYN packet.\n");
        // send syn ack
        printf("Sending SYN-ACK packet.\n");
        int sent = rudp_send(sockfd, SYN_ACK, NULL, 0);
        if (sent == -1)
        {
            printf("Failed to send SYN-ACK packet.\n");
            return 0;
        }
    }

    // receive ack
    recv = rudp_receive(sockfd, &packet);
    if (recv == -1)
    {
        printf("Failed to receive ACK packet.\n");
        return 0;
    }

    if (packet.header.flags == ACK)
    {
        printf("Received ACK packet.\n");
        sockfd->isConnected = true;
    }
    else
    {
        printf("Received unexpected packet.\n");
        return 0;
    }
    printf("Connected to %s:%d\n", inet_ntoa(sockfd->dest_addr.sin_addr), ntohs(sockfd->dest_addr.sin_port));
    return 1;
}

// Receives data from the other side and put it into the buffer.
// Returns the number of received bytes on success, 0 if got FIN packet (disconnect), and -1 on error.
int rudp_receive(RUDP_Socket *rudp_socket, RUDP_Packet *packet)
{
    size_t total_received = 0;
    socklen_t addr_len = sizeof(rudp_socket->dest_addr);

    if (rudp_socket->isServer)
    {

        while (total_received < BUFFER_SIZE)
        {
            int bytes_received = recvfrom(rudp_socket->socket_fd, packet, sizeof(RUDP_Packet), 0, (struct sockaddr *)&rudp_socket->dest_addr, &addr_len);
            if (bytes_received < 0)
            {
                printf("%s:%d\n", inet_ntoa(rudp_socket->dest_addr.sin_addr), ntohs(rudp_socket->dest_addr.sin_port));
                perror("recvfrom");
                return -1;
            }

            size_t data_size = bytes_received - sizeof(RUDP_Header);

            if (packet->header.flags == SYN || packet->header.flags == SYN_ACK || packet->header.flags == ACK || packet->header.flags == FIN_ACK)
            {
                // Ignore control packets (SYN, SYN-ACK, ACK, FIN)
                break;
            }
            else if (packet->header.flags == FIN)
            {
                return 0;
            }
            else if (packet->header.flags == PUSH)
            {
                // Check if the received packet is corrupted
                unsigned short int checksum = calculate_checksum(packet->data, data_size);
                if (checksum != packet->header.checksum)
                {
                    printf("Checksum failed for sequence number %d: %d\n", packet->header.sequence_number, checksum);
                    printf("seq %d, len %d, chek %d, ack %d, f %d\n", packet->header.sequence_number, packet->header.length, packet->header.checksum, packet->header.acknowledgment_number, packet->header.flags);
                    continue;
                }
            }

            total_received += data_size;

            // Check if all data has been received
            if (total_received >= BUFFER_SIZE)
            {
                break;
            }
        }
    }
    else
    {
        // only receive connection packets
        int bytes_received = recvfrom(rudp_socket->socket_fd, packet, sizeof(RUDP_Packet), 0, (struct sockaddr *)&rudp_socket->dest_addr, &addr_len);
        if (bytes_received < 0)
        {
            perror("recvfrom");
            return -1;
        }
        if (packet->header.flags == SYN || packet->header.flags == SYN_ACK || packet->header.flags == ACK || packet->header.flags == FIN_ACK)
        {
            return bytes_received;
        }
        else if (packet->header.flags == FIN)
        {
            return 0;
        }
    }
    return total_received;
}

// Sends data stores in buffer to the other side.
// Returns the number of sent bytes on success and -1 on error.
int rudp_send(RUDP_Socket *rudp_socket, uint8_t flags, char *data, size_t data_size)
{

    RUDP_Packet packet;
    packet.header.flags = flags;

    if (flags == SYN || flags == SYN_ACK || flags == ACK || flags == FIN || flags == FIN_ACK)
    {
        // If SYN, SYN-ACK, ACK, FIN, or FIN-ACK flags are set, send packet with header only
        packet.header.length = sizeof(RUDP_Header);
        packet.header.sequence_number = 0;       // Set sequence number
        packet.header.acknowledgment_number = 0; // Set appropriate acknowledgment number
        packet.header.checksum = 0;              // Set checksum to 0

        if (sendto(rudp_socket->socket_fd, (const char *)&packet, sizeof(RUDP_Header), 0,
                   (struct sockaddr *)&rudp_socket->dest_addr, (socklen_t)sizeof(rudp_socket->dest_addr)) == -1)
        {
            return -1; // Return -1 on failure
        }
    }
    else
    {
        // If data packet, split data into chunks and send
        size_t total_sent = 0;
        int sequence_number = 0;
        while (total_sent < data_size)
        {
            size_t remaining = data_size - total_sent;
            size_t chunk_size = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;

            packet.header.length = sizeof(RUDP_Header) + chunk_size;
            packet.header.sequence_number = sequence_number; // Set sequence number
            packet.header.acknowledgment_number = 0;         // Set appropriate acknowledgment number
            memcpy(packet.data, data + total_sent, chunk_size);
            packet.header.checksum = calculate_checksum(packet.data, chunk_size);

            // Send the packet
            if (sendto(rudp_socket->socket_fd, (const char *)&packet, sizeof(RUDP_Header) + chunk_size, 0,
                       (struct sockaddr *)&rudp_socket->dest_addr, (socklen_t)sizeof(rudp_socket->dest_addr)) == -1)
            {
                return -1; // Return -1 on failure
            }

            // print sent packet info
            // printf("seq %d, len %d, chek %d\n", packet.header.sequence_number, packet.header.length, packet.header.checksum);

            // Increment sequence number by the size of the chunk sent
            sequence_number++;

            total_sent += chunk_size;
        }
    }

    return data_size; // Return the size of the data sent on success
}

// Disconnects from an actively connected socket.
// Returns 1 on success, 0 when the socket is already disconnected (failure).
int rudp_disconnect(RUDP_Socket *sockfd)
{
    if (!sockfd->isConnected)
    {
        printf("Socket is already disconnected.\n");
        return 0;
    }

    if (!sockfd->isServer)
    {
        // send fin
        printf("Sending FIN packet.\n");
        int sent = rudp_send(sockfd, FIN, NULL, 0);
        if (sent == -1)
        {
            return 0;
        }

        // receive fin ack
        RUDP_Packet packet;
        int recv = rudp_receive(sockfd, &packet);
        if (recv == -1)
        {
            return 0;
        }
        if (packet.header.flags == FIN_ACK)
        {
            printf("Received FIN-ACK packet.\n");
            sockfd->isConnected = false;
        }
        else
        {
            printf("Received unexpected packet.\n");
            printf("Received %d\n", packet.header.flags);
            return 0;
        }

        // send ack
        printf("Sending ACK packet.\n");
        sent = rudp_send(sockfd, ACK, NULL, 0);
        if (sent == -1)
        {
            return 0;
        }
    }
    return 1;
}

// This function releases all the memory allocation and resources of the socket.
int rudp_close(RUDP_Socket *sockfd)
{
    close(sockfd->socket_fd);
    free(sockfd);
    return 1;
}