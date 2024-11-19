#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080           // Port the server listens on
#define BUFFER_SIZE 1024    // Buffer size for communication

// Define the structure for trivia questions
typedef struct {
    char question[BUFFER_SIZE];
    char answer[BUFFER_SIZE];
} Trivia;

// Sample trivia questions
Trivia trivia[] = {
    {"What is the capital of France?", "Paris"},
    {"What is 5 + 7?", "12"},
    {"Who wrote 'Romeo and Juliet'?", "Shakespeare"}
};
int trivia_count = 3; // Number of trivia questions

// Function to handle communication with a single client
void handle_client(int client_socket, char* client_ip) {
    char buffer[BUFFER_SIZE]; // Buffer for communication
    int score = 0;           // Client's score

    for (int i = 0; i < trivia_count; i++) {
        // Send the current question to the client
        send(client_socket, trivia[i].question, strlen(trivia[i].question), 0);

        // Allow time for the client to process the question
        usleep(100000); // 100ms delay

        // Receive the client's answer
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            printf("Client @ %s disconnected or error occurred.\n", client_ip);
            break;
        }
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        printf("Received from client @ %s: '%s'\n", client_ip, buffer);

        // Remove trailing newline if present
        buffer[strcspn(buffer, "\n")] = 0;

        // Check the client's answer and send feedback
        if (strcmp(buffer, trivia[i].answer) == 0) {
            score++;
            send(client_socket, "Correct!\n", 9, 0);
        } else {
            send(client_socket, "Wrong!\n", 7, 0);
        }

        // Allow time for the client to process the feedback
        usleep(100000); // 100ms delay
    }

    // Send the final score to the client
    sprintf(buffer, "Your final score is: %d\n", score);
    send(client_socket, buffer, strlen(buffer), 0);

    close(client_socket); // Close the connection with the client
}

int main() {
    int server_socket, client_socket; // Socket descriptors
    struct sockaddr_in server_addr, client_addr; // Address structures
    socklen_t addr_len = sizeof(client_addr);
    char client_ip[INET_ADDRSTRLEN]; // Buffer to store the client's IP address

    // Create the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure the server's address
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // Accept connections on any IP
    server_addr.sin_port = htons(PORT); // Port

    // Bind the server socket to the configured address
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    listen(server_socket, 5);
    printf("Server is running on port %d...\n", PORT);

    while (1) {
        // Accept a client connection
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        // Convert the client's IP address to a human-readable format
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        printf("Client connected: %s\n", client_ip);

        // Handle the client in a single session
        handle_client(client_socket, client_ip);

        printf("Client disconnected: %s\n", client_ip);
    }

    close(server_socket); // Close the server socket
    return 0;
}
