#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>


#define BUFFER_SIZE 2 * 1024 * 1024

int main(int argc, char *argv[]) {

    char *server_ip;
    char *algorithm;
    int server_port;


    if(argc != 7){
        fprintf(stderr, "Usage: %s -ip <server_ip> -p <server_port> -algo <algorithm>\n", argv[0]);
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
        }else if (strcmp(argv[i], "-algo") == 0)
        {
            algorithm = argv[i+1];
        }
    }
    

    fprintf(stdout, "Starting Sender...\n");

    
    // The variable to store the socket file descriptor.
    int sock = -1;

    // The variable to store the server's address.
    struct sockaddr_in receiver_addr;

    // Reset the receiver_addr to zero
    memset(&receiver_addr, 0, sizeof(receiver_addr));

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
    if ((bytes_read = fread(file_data, sizeof(char), BUFFER_SIZE, file)) != BUFFER_SIZE) {
        perror("fread(3)");
        fclose(file);
        free(file_data);
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "bytes read: %d.\n", bytes_read);

    // Close the file
    fclose(file);


    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock <= 0) {
        perror("socket(2)");
        exit(EXIT_FAILURE);
    }

    // Conver the IP address from text to binary form
    if (inet_pton(AF_INET, server_ip, &receiver_addr.sin_addr) <= 0) {
        perror("inet_pton(3)");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Set the congestion control algorithm.
    if(setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, algorithm, strlen(algorithm)) < 0){
        perror("setsockopt(2)");
        close(sock);
        exit(EXIT_FAILURE);
    }
    
    // Set the server's address family to AF_INET (IPv4).
    receiver_addr.sin_family = AF_INET;
    // Set the server's port number.
    receiver_addr.sin_port = htons(server_port);

    fprintf(stdout, "Waiting for TCP connection...\n");
    // Connect to receiver
    if (connect(sock, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr)) < 0) {
        perror("Connect(2)");
        close(sock);
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, "Receiver connected, beginning to receive file...\n");

    char decision;
    do {
        
        // Send the file
        int bytes_sent = send(sock, file_data, BUFFER_SIZE, 0);
        if (bytes_sent <= 0) {
            perror("send(2)");
            close(sock);
            free(file_data);
            exit(EXIT_FAILURE);
        }
    
        fprintf(stdout, "File sent.\n");
        fprintf(stdout, "Bytes sent: %d\n", bytes_sent);
        fprintf(stdout, "Waiting for Receiver response...\n");

        //Receive response from the receiver
        char rec_buffer[1024];
        int bytes_received = recv(sock, rec_buffer, 1024, 0);
        if (bytes_received <= 0) {
            perror("recv(2)");
            close(sock);
            free(file_data);
            exit(EXIT_FAILURE);
        }

        //User decision: Send the file again or close the connection
        fprintf(stdout, "Do you want to send the file again? (y/n): ");
        scanf(" %c", &decision);

        } while (decision == 'Y' || decision == 'y');
        
    free(file_data);

    //Send an exit message to the receiver
    char *exit_message = "exit";
    if (send(sock, exit_message, strlen(exit_message) + 1, 0) < 0) {
        perror("send(2)");
        close(sock);
        exit(EXIT_FAILURE);
    }

    //Close the TCP connection
    close(sock);
    fprintf(stdout, "Connection closed\n");
    fprintf(stdout, "Sender end\n");
    
    return 0;
}




