#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

int main()
{
    int client_socket;
    struct sockaddr_in server_addr;
    char server_ip[] = "127.0.0.1"; // Change this to the server IP address
    int server_port = 8080;         // Change this to the server port number
    char message[BUFFER_SIZE];

    // Create client socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0)
    {
        perror("Error creating client socket");
        exit(1);
    }

    // Prepare server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(server_port);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error connecting to the server");
        exit(1);
    }

    printf("Connected to the server. Type 'exit' to quit.\n");

    // Send and receive data
    while (1)
    {
        printf("Enter message: ");
        fgets(message, sizeof(message), stdin);

        // Remove newline character from the input
        message[strcspn(message, "\n")] = '\0';

        // Send the message to the server
        send(client_socket, message, strlen(message), 0);

        // Check if the client wants to exit
        if (strcmp(message, "exit") == 0)
            break;

        // Receive and display the server's response
        memset(message, 0, sizeof(message));
        int bytes_received = recv(client_socket, message, sizeof(message), 0);
        if (bytes_received <= 0)
        {
            perror("Error receiving data from the server");
            exit(1);
        }

        printf("Server response: %s\n", message);
    }

    close(client_socket);
    printf("Disconnected from the server.\n");

    return 0;
}
