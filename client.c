#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1" // The server's IP address (use Raspberry Pi server IP in actual setup)
#define SERVER_PORT 8080      // Port the server is listening on
#define BUFFER_SIZE 1024      // Buffer size for messages

// Function to clear residual input in stdin
void clear_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF); // Discard any leftover input
}

int main() {
    int client_socket; // Socket descriptor for the client
    struct sockaddr_in server_addr; // Server address structure
    char buffer[BUFFER_SIZE];       // Buffer for communication
    int n;

    // Create the socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure the server's address
    server_addr.sin_family = AF_INET;                   // IPv4
    server_addr.sin_port = htons(SERVER_PORT);          // Port
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP); // IP address

    // Connect to the server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to server failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    printf("Connected to Trivia Game Server!\n");

    while (1) {
        // Receive a message (question or final score) from the server
        memset(buffer, 0, BUFFER_SIZE);
        n = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (n <= 0) {
            printf("Server disconnected.\n");
            break;
        }

        // Check if the received message is the final score
        if (strstr(buffer, "Your final score")) {
            printf("Game Over: %s\n", buffer);
            break; // Exit the game loop on final score
        } else {
            // If it's a question, display it
            printf("Question: %s\n", buffer);

            // Prompt the user for an answer
            printf("Your Answer: ");
            fflush(stdin);
            fgets(buffer, BUFFER_SIZE, stdin);
            buffer[strcspn(buffer, "\n")] = 0; // Remove newline character

            // Send the answer to the server
            send(client_socket, buffer, strlen(buffer), 0);

            // Receive feedback from the server (e.g., Correct!/Wrong!)
            memset(buffer, 0, BUFFER_SIZE);
            n = recv(client_socket, buffer, BUFFER_SIZE, 0);

            if (n > 0) {
                printf("%s\n", buffer);
            }
        }
    }

    close(client_socket); // Close the connection
    return 0;
}

