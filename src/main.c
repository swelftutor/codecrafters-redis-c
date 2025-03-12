#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> // For inet_ntoa (optional, for printing IP addresses)
#include <sys/wait.h>   // For waitpid()

#define BUFFER_SIZE 1024
#define PORT 6379

// Function to handle communication with a single client
void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(client_fd, buffer, BUFFER_SIZE)) > 0) {
        // Respond with "+PONG\r\n"
        if (send(client_fd, "+PONG\r\n", 7, 0) == -1) {
            perror("send failed");
            break; // Exit the loop on send failure
        }
    }

    if (bytes_read == -1) {
        perror("read failed"); // Use perror for more informative error messages
    }

    // The client has disconnected (or an error occurred)
    // No need to explicitly close client_fd here; it's done in the main loop
}

// Signal handler for SIGCHLD to prevent zombie processes
void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main() {
    setbuf(stdout, NULL); // Disable buffering
    setbuf(stderr, NULL);

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set SO_REUSEADDR
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    // Prepare the server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 5) != 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

	// Install signal handler for SIGCHLD
    struct sigaction sa;
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // Main server loop
    while (1) {
        client_addr_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            perror("accept failed");
            continue; // Go back to the beginning of the loop
        }

        // Print client information (optional, but helpful for debugging)
        printf("Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            close(client_fd); // Close the client socket in the parent
            continue;
        }

        if (pid == 0) { // Child process
            close(server_fd); // Close the listening socket in the child
            handle_client(client_fd);
            close(client_fd); // Close the client socket in the child
            exit(EXIT_SUCCESS); // Exit the child process cleanly
        } else { // Parent process
            close(client_fd); // Close the client socket in the parent
        }
    }

    close(server_fd); // Close the server socket (this will only be reached if the loop breaks)
    return 0;
}