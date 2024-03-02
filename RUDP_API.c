#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <stdbool.h>

#define SERVER_IP "127.0.0.1"

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

// A struct that represents RUDP Socket
typedef struct _rudp_socket
{
    int socket_fd;                // UDP socket file descriptor
    bool isServer;                // True if the RUDP socket acts like a server, false for client.
    bool isConnected;             // True if there is an active connection, false otherwise.
    struct sockaddr_in dest_addr; // Destination address. Client fills it when it connects via rudp_connect(), server fills it when it accepts a connection via rudp_accept().
} RUDP_Socket;

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

    int UdpSock = -1;

    UdpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (UdpSock < 0)
    {
        perror("socket(2)");
        free(sockfd);
        exit(EXIT_FAILURE);
    }

    sockfd->socket_fd = UdpSock;
    sockfd->isServer = isServer;
    sockfd->isConnected = false;

    if (isServer)
    {
        if (bind(UdpSock, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
            perror("bind(2)");
            close(UdpSock);
            free(sockfd);
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
    // Connect to receiver
    if (connect(sockfd->socket_fd, (struct sockaddr *)&sockfd->dest_addr, sizeof(sockfd->dest_addr)) < 0)
    {
        perror("Connect(2)");
        close(sockfd->socket_fd);
        exit(EXIT_FAILURE);
    }
    sockfd->isConnected = true;
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
    struct sockaddr_in client;
    int client_len = sizeof(client);
    int new_socket = accept(sockfd->socket_fd, (struct sockaddr *)&client, (socklen_t *)&client_len);
    if (new_socket < 0)
    {
        perror("accept(2)");
        return 0;
    }
    sockfd->dest_addr = client;
    sockfd->isConnected = true;
    return 1;
}

// Receives data from the other side and put it into the buffer.
// Returns the number of received bytes on success, 0 if got FIN packet (disconnect), and -1 on error.
// Fails if called when the socket is disconnected.
int rudp_recv(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size)
{
    if (!sockfd->isConnected)
    {
        return -1;
    }
    int recv_size = recv(sockfd->socket_fd, buffer, buffer_size, 0);
    if (recv_size < 0)
    {
        perror("recv(2)");
        return -1;
    }
    if (recv_size == 0)
    {
        sockfd->isConnected = false;
        return 0;
    }
    return recv_size;
}

// Sends data stores in buffer to the other side.
// Returns the number of sent bytes on success, 0 if got FIN packet (disconnect), and -1 on error.
// Fails if called when the socket is disconnected.
int rudp_send(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size)
{
    if (!sockfd->isConnected)
    {
        return -1;
    }
    int send_size = send(sockfd->socket_fd, buffer, buffer_size, 0);
    if (send_size < 0)
    {
        perror("send(2)");
        return -1;
    }
    if (send_size == 0)
    {
        sockfd->isConnected = false;
        return 0;
    }
    return send_size;
}

// Disconnects from an actively connected socket.
// Returns 1 on success, 0 when the socket is already disconnected (failure).
int rudp_disconnect(RUDP_Socket *sockfd)
{
    if (!sockfd->isConnected)
    {
        return 0;
    }
    close(sockfd->socket_fd);
    sockfd->isConnected = false;
    return 1;
}

// This function releases all the memory allocation and resources of the socket.
int rudp_close(RUDP_Socket *sockfd)
{
    close(sockfd->socket_fd);
    free(sockfd);
    return 1;
}