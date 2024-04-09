#include "RUDP_API.c"

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

        // send the file to the receiver
        if (rudp_send(sock, PUSH, file_data, bytes_read) < 0)
        {
            fprintf(stderr, "Failed to send the file.\n");
            rudp_close(sock);
            free(file_data);
            exit(EXIT_FAILURE);
        }

        fprintf(stdout, "File sent.\n");

        // receive response packet from the receiver
        if (rudp_receive(sock, &rec_packet) < 0)
        {
            fprintf(stderr, "Failed to receive response packet.\n");
            rudp_close(sock);
            free(file_data);
            exit(EXIT_FAILURE);
        }

        printf("do you want to send another file? (Y/N): ");
        scanf(" %c", &decision);
    } while (decision == 'Y' || decision == 'y');

    // disconnect from the receiver and close the socket
    int d = rudp_disconnect(sock);
    if (d == 0){
        fprintf(stderr, "Failed to disconnect from the receiver.\n");
        return 1;
    }
    printf("Disconnected from %s:%d\n", inet_ntoa(sock->dest_addr.sin_addr), ntohs(sock->dest_addr.sin_port));
    rudp_close(sock);
    free(file_data);
    return 0;
}