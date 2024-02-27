#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define SERTVER_PORT 8080
#define SERVER_IP "127.0.0.1"

#define BUFFER_SIZE 2 * 1024 * 1024



/*
* @brief A random data generator function based on srand() and rand().
* @param size The size of the data to generate (up to 2^32 bytes).
* @return A pointer to the buffer.
*/
char *util_generate_random_data(unsigned int size) {
    char *buffer = NULL;
    // Argument check.
    if (size == 0){
        return NULL;
    }

    buffer = (char *)calloc(size, sizeof(char));

    // Error checking.
    if (buffer == NULL){
        return NULL;
    }
    
    // Randomize the seed of the random number generator.
    srand(time(NULL));

    for (unsigned int i = 0; i < size; i++){
        *(buffer + i) = ((unsigned int)rand() % 256);
    }
    
    return buffer;
}




int main() {
    fprintf(stdout, "Starting Sender...\n");

    
    // The variable to store the socket file descriptor.
    int sock = -1;

    // The variable to store the server's address.
    struct sockaddr_in receiver_addr;

    // Reset the receiver_addr to zero
    memset(&receiver_addr, 0, sizeof(receiver_addr));


    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock <= 0) {
        perror("socket(2)");
        exit(EXIT_FAILURE);
    }

    // Conver the IP address from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &receiver_addr.sin_addr) <= 0) {
        perror("inet_pton(3)");
        close(sock);
        exit(EXIT_FAILURE);
    }
    
    // Set the server's address family to AF_INET (IPv4).
    receiver_addr.sin_family = AF_INET;
    // Set the server's port number.
    receiver_addr.sin_port = htons(SERTVER_PORT);

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
        
        //Create file
        char *file = util_generate_random_data(BUFFER_SIZE);
        // Send the file
        int bytes_sent = send(sock, file, sizeof(BUFFER_SIZE), 0);
        if (bytes_sent <= 0) {
            perror("send(2)");
            close(sock);
            free(file);
            exit(EXIT_FAILURE);
        }
    
    fprintf(stdout, "File sent.\n");
    fprintf(stdout, "Waiting for Receiver response...\n");

        //Receive the file
        char rec_buffer[1024];
        int bytes_received = recv(sock, rec_buffer, sizeof(rec_buffer), 0);
        if (bytes_received <= 0) {
            perror("recv(2)");
            close(sock);
            free(file);
            exit(EXIT_FAILURE);
        }

        //User decision: Send the file again or close the connection
        fprintf(stdout, "Do you want to send the file again? (y/n): ");
        scanf("%c", &decision);

        } while (decision == 'Y' || decision == 'y');
        

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




