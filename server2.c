#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 8080        // Port the server listens on 
#define BUFFER_SIZE 1024 // Buffer size for communication
#define MAX_ENTRIES 100
#define MAX_TOP_SCORES 5

//Define the leaderboard entry structure
typedef struct{
	char ip[100];
	int score;
}leaderScores;

// Define the structure for trivia questions
typedef struct {
	char question[BUFFER_SIZE];
	char answer[BUFFER_SIZE];
} Trivia;

// Sample trivia questions
Trivia trivia[] = {{"What is the capital of France?", "Paris"},
                   {"What is 5 + 7?", "12"},
                   {"Who wrote 'Romeo and Juliet'?", "Shakespeare"}};

int trivia_count = 3; // Number of trivia questions

int compare(const void *a, const void *b){
	leaderScores *entryA= (leaderScores *)a;
	leaderScores *entryB= (leaderScores *)b;
	return entryB->score - entryA->score; //descending order
}

void saveScore(const char *client_ip, int score){
	FILE *file = fopen("leaderboard.txt", "a");
        if (file==NULL){
        	perror("Cannot open the leaderboard");
		return;
	}

	fprintf(file, "%s|%d\n", client_ip, score);
	fclose(file);
}

void loadScore(){
	FILE *file = fopen("leaderboard.txt", "r");
	if (file==NULL){
		perror("Cannot open the leaderboard");
		return;
	}	       
	leaderScores leaderboard[MAX_ENTRIES];
	int count=0;
		
	//read and populate array
	while (count<MAX_ENTRIES && fscanf(file, "%[^|]|%d\n", leaderboard[count].ip, &leaderboard[count].score) != EOF) {
		count++;
	}
	fclose(file);
	
	//sorts leaderboard in descending order
        qsort(leaderboard, count, sizeof(leaderScores), compare);

	//shows sorted leaderboard
	printf("Updated leaderboard:\n");
	for (int i=0; i<MAX_TOP_SCORES; i++){
		if (i<count){
			printf("%d. %s: %d\n", i + 1, leaderboard[i].ip, leaderboard[i].score);
		}else{
			//placeholder for empty positions
			printf("%d. [empty]\n", i + 1);
		}
	}
}

// Function to handle communication with a single client
void handle_client(int client_socket, char *client_ip){
	char buffer[BUFFER_SIZE]; // Buffer for communication
	int score = 0;   

	loadScore();
	
	for (int i = 0; i < trivia_count; i++) {
		// Send the current question to the client
		send(client_socket, trivia[i].question, strlen(trivia[i].question), 0);
		// Allow time for the client to process the question
	       	usleep(100000); // 100ms delay
	
		// Receive the client's answer
		memset(buffer, 0, BUFFER_SIZE);
		int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
		if (bytes_received <= 0) {
			printf("Client disconnected or error occurred.\n");
			break;
		}	
	
		buffer[bytes_received] = '\0'; // Null-terminate the received data
		buffer[strcspn(buffer, "\n")] = 0; // Remove trailing newline if present
		printf("Received from client: '%s'\n", buffer);
	
		// Check the client's answer and send feedback
		if (strcmp(buffer, trivia[i].answer) == 0) {
			score++;
			send(client_socket, "Correct!\n", 9, 0);
		} else {
			send(client_socket, "Wrong!\n", 7, 0);
		}
		usleep(100000); // 100ms delay
	}

	// Send the final score to the client
	sprintf(buffer, "Your final score is: %d\n", score);
	send(client_socket, buffer, strlen(buffer), 0);
	close(client_socket); // Close the connection with the client
	                                                                         
	saveScore(client_ip, score);
	loadScore();
}

int main() {
	int server_socket, client_socket;            // Socket descriptors
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
	server_addr.sin_family = AF_INET; //IPv4
	server_addr.sin_addr.s_addr = INADDR_ANY; // Accept connections on any IP
	server_addr.sin_port = htons(PORT);       // Port
	
	// Bind the server socket to the configured address
	if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("Bind failed");
		close(server_socket);
		exit(EXIT_FAILURE);
	}
			 
	//Listen for incoming connections
	listen(server_socket, 5);
	printf("Server is running on port %d...\n", PORT);
	
	while (1) {
		// Accept a client connection
		client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
	        if (client_socket < 0) {
			perror("Accept failed");
			continue;
		}
		
		// Convert the client's IP address to a human-readable format */
		inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
		printf("Client connected: %s\n", client_ip);
		
		// Handle the client in a thread
		handle_client(client_socket, client_ip);

		printf("Client disconnected: %s\n", client_ip);
	}
	close(server_socket); // Close the server socket
	return 0;
}
