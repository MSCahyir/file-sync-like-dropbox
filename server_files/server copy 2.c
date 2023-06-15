#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

int clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t client_mutex;

void *handle_client(void *arg)
{
    int client_socket = *((int *)arg);
    char buffer[BUFFER_SIZE];

    // Receive and send data
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0)
            break;

        // Process the received data

        // Send response
        send(client_socket, buffer, strlen(buffer), 0);
    }

    // Client disconnected
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < client_count; ++i)
    {
        if (clients[i] == client_socket)
        {
            memmove(clients + i, clients + i + 1, (client_count - i - 1) * sizeof(int));
            break;
        }
    }
    --client_count;
    pthread_mutex_unlock(&client_mutex);

    close(client_socket);
    pthread_exit(NULL);
}

void start_server(int port)
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("Error creating server socket");
        exit(1);
    }

    // Prepare server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind server socket to the specified address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error binding server socket");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) < 0)
    {
        perror("Error listening for connections");
        exit(1);
    }

    printf("Server started. Listening on port %d...\n", port);

    fd_set readfds;
    int max_fd, activity;
    struct timeval timeout;

    // Initialize the file descriptor set
    FD_ZERO(&readfds);

    // Add server socket to the set
    FD_SET(server_socket, &readfds);
    max_fd = server_socket + 1;

    pthread_t thread_id;

    // Accept and handle client connections
    while (1)
    {
        // Set the file descriptors for select
        fd_set tmp_fds = readfds;

        // Set the timeout value (1 second)
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // Wait for events on file descriptors
        activity = select(max_fd, &tmp_fds, NULL, NULL, &timeout);
        if (activity == -1)
        {
            perror("Error in select");
            exit(1);
        }
        else if (activity == 0)
        {
            // Timeout occurred, do something if needed
            continue;
        }

        // Check if the server socket has a new connection
        if (FD_ISSET(server_socket, &tmp_fds))
        {
            client_addr_len = sizeof(client_addr);
            client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
            if (client_socket < 0)
            {
                perror("Error accepting client connection");
                exit(1);
            }

            // Check if the maximum number of clients has been reached
            if (client_count >= MAX_CLIENTS)
            {
                printf("Maximum number of clients reached. Connection rejected.\n");
                close(client_socket);
                continue;
            }

            // Add the client to the client list
            pthread_mutex_lock(&client_mutex);
            clients[client_count++] = client_socket;
            pthread_mutex_unlock(&client_mutex);

            // Add the client socket to the file descriptor set
            FD_SET(client_socket, &readfds);
            if (client_socket >= max_fd)
                max_fd = client_socket + 1;

            // Create a new thread to handle the client
            if (pthread_create(&thread_id, NULL, handle_client, &clients[client_count - 1]) != 0)
            {
                perror("Error creating client thread");
                exit(1);
            }

            // Detach the thread to clean up resources automatically
            pthread_detach(thread_id);

            printf("Client connected. Client IP: %s\n", inet_ntoa(client_addr.sin_addr));
        }

        // Check if any client socket has an incoming request
        for (int i = 0; i < client_count; ++i)
        {
            client_socket = clients[i];
            if (FD_ISSET(client_socket, &tmp_fds))
            {
                // Handle the client request
                // ...
            }
        }
    }

    close(server_socket);
}

int main()
{
    int port = 8080; // Specify the desired port number
    pthread_mutex_init(&client_mutex, NULL);
    start_server(port);
    pthread_mutex_destroy(&client_mutex);
    return 0;
}
