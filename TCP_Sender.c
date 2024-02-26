#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


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
    //Read the created file
    FILE *file = util_generate_random_data(2 * 1024 * 1024);
    

    //Create a TCP socket between the sender and the receiver
    int sockfd;
    struct sockaddr_in sender_addr, receiver_addr;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up sender address
    memset(&sender_addr, 0, sizeof(sender_addr));
    sender_addr.sin_family = AF_INET;
    sender_addr.sin_addr.s_addr = INADDR_ANY;
    sender_addr.sin_port = htons(8080);

    // Bind socket to sender address
    if (bind(sockfd, (struct sockaddr *)&sender_addr, sizeof(sender_addr)) < 0) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    // Set up receiver address
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    receiver_addr.sin_port = htons(8080);

    // Connect to receiver
    if (connect(sockfd, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    //Send the file
    // Send the file
    if (send(sockfd, file, sizeof(file), 0) < 0) {
        perror("File sending failed");
        exit(EXIT_FAILURE);
    }
    

    //User decision: Send the file again or close the connection
    char decision;
    printf("Do you want to send the file again? (y/n): ");
    scanf("%c", &decision);
    if (decision == 'y') {
        // Send the file again
        if (send(sockfd, file, sizeof(file), 0) < 0) {
            perror("File sending failed");
            exit(EXIT_FAILURE);
        }
    }

    //Send an exit message to the receiver
    char *exit_message = "exit";
    if (send(sockfd, exit_message, strlen(exit_message), 0) < 0) {
        perror("Exit message sending failed");
        exit(EXIT_FAILURE);
    }

    //Close the TCP connection
    close(sockfd);

    return 0;
}




