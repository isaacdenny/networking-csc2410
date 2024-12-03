#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 8080        // Port the server listens on
#define BUFFER_SIZE 1024 // Buffer size for communication
#define MAX_ENTRIES 100
#define MAX_TOP_SCORES 5

typedef struct {
  char ip[100];
  int score;
} Score;

typedef struct {
  char question[BUFFER_SIZE];
  char answer[BUFFER_SIZE];
} Trivia;

typedef struct {
  int *client_socket;
  char ip[INET_ADDRSTRLEN];
} client_info;

Trivia trivia[50];
int trivia_count = 0;

Score leaderboard[MAX_ENTRIES];
char lb_buffer[1024]; // 12 chars per line * 50 lines < 1024
int score_count = 0;

// loads trivia questions from file
void load_trivia() {
  FILE *fptr;
  trivia_count = 0;
  int bufferLen = 256; // read

  fptr = fopen("questions.txt", "r");

  assert(fptr != NULL); // if cannot open file, program should end

  char buffer[bufferLen];
  while (fgets(buffer, bufferLen, fptr)) {
    char *question = strtok(buffer, "|"); // read token until |
    char *answer = strtok(NULL, "|");     // continue reading for answer
    strncpy(trivia[trivia_count].question, question, strlen(question));
    strncpy(trivia[trivia_count].answer, answer,
            strlen(answer) - 1); // ignore new line char
    trivia_count++;
  }
}

int compare(const void *a, const void *b) {
  Score *entryA = (Score *)a;
  Score *entryB = (Score *)b;
  return entryB->score - entryA->score; // descending order
}

void load_scores() {
  FILE *file = fopen("leaderboard.txt", "a+");
  if (file == NULL) {
    perror("Error opening file");
    return;
  }

  score_count = 0;
  // read and populate array
  while (score_count < MAX_ENTRIES &&
         fscanf(file, "%[^|]|%d\n", leaderboard[score_count].ip,
                &leaderboard[score_count].score) != EOF) {
    score_count++;
  }
  fclose(file);

  // sorts leaderboard in descending order
  qsort(leaderboard, score_count, sizeof(Score), compare);

  int offset = 0;
  int buffer_size = sizeof(lb_buffer);

  offset += snprintf(lb_buffer, buffer_size, "%s\n", "IP|Score");

  // Format the leaderboard into a string for sending to client
  for (int i = 0; i < score_count && i < MAX_TOP_SCORES; i++) {
    offset += snprintf(lb_buffer + offset, buffer_size - offset, "%s|%d\n",
                       leaderboard[i].ip, leaderboard[i].score);
    if (offset >= sizeof(lb_buffer)) {
      break; // Prevent overflow
    }
  }

  printf("%s", lb_buffer);
}

void save_score(const char *client_ip, int score) {
  assert(score_count < MAX_ENTRIES);

  strncpy(leaderboard[score_count].ip, client_ip, INET_ADDRSTRLEN);
  leaderboard[score_count].score = score;
  score_count++;

  FILE *file = fopen("leaderboard.txt", "a");
  if (file == NULL) {
    perror("Cannot open the leaderboard");
    return;
  }

  fprintf(file, "%s|%d\n", client_ip, score);

  fclose(file);
  load_scores();
}

// handle communication with a single client
void *handle_client(void *args) {
  client_info *targs = (client_info *)args;

  assert(targs != NULL);
  assert(*targs->client_socket > 0);

  char buffer[BUFFER_SIZE];
  int score = 0;

  // send the leaderboard
  send(*targs->client_socket, lb_buffer, strlen(lb_buffer), 0);
  usleep(100000);

  for (int i = 0; i < trivia_count; i++) {
    send(*targs->client_socket, trivia[i].question, strlen(trivia[i].question),
         0);

    // Allow time for the client to process the question
    usleep(100000);

    memset(buffer, 0, BUFFER_SIZE);
    int bytes_received =
        recv(*targs->client_socket, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received <= 0) {
      printf("Client disconnected or error occurred.\n");
      break;
    }

    buffer[bytes_received] = '\0'; // Null-terminate the received data
    printf("Received from client: '%s'\n", buffer);

    // Remove trailing newline if present
    buffer[strcspn(buffer, "\n")] = 0;

    if (strcmp(buffer, trivia[i].answer) == 0) {
      score++;
      send(*targs->client_socket, "Correct!\n", 9, 0);
    } else {
      send(*targs->client_socket, "Wrong!\n", 7, 0);
    }

    // Allow time for the client to process the feedback
    usleep(100000); // 100ms delay
  }

  // Send the final score to the client
  sprintf(buffer, "Your final score is: %d\n", score);
  send(*targs->client_socket, buffer, strlen(buffer), 0);

  close(*targs->client_socket);
  save_score(targs->ip, score);

  return NULL;
}

int main() {

  load_trivia();
  load_scores();

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
  server_addr.sin_family = AF_INET;         // IPv4
  server_addr.sin_addr.s_addr = INADDR_ANY; // Accept connections on any IP
  server_addr.sin_port = htons(PORT);       // Port

  // Bind the server socket to the configured address
  if (bind(server_socket, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    perror("Bind failed");
    close(server_socket);
    exit(EXIT_FAILURE);
  }

  // Listen for incoming connections
  listen(server_socket, 5);
  printf("Server is running on port %d...\n", PORT);

  while (1) {
    // Accept a client connection
    client_socket =
        accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);

    if (client_socket < 0) {
      perror("Accept failed");
      continue;
    }

    pthread_t thread_id;

    // Convert the client's IP address to a human-readable format
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    printf("Client connected: %s\n", client_ip);

    client_info *args = malloc(sizeof(client_info));
    int* client_socket_ptr = malloc(sizeof(int));
    *client_socket_ptr = client_socket;
    args->client_socket = client_socket_ptr;
    strncpy(args->ip, client_ip, INET_ADDRSTRLEN);

    // Handle the client in a thread
    if (pthread_create(&thread_id, NULL, handle_client, args) != 0) {
      perror("Thread creation failed");
      free(args);
      close(client_socket);
    } else {
      pthread_detach(thread_id);
    }

    printf("Client disconnected: %s\n", client_ip);
  }

  close(server_socket); // Close the server socket
  return 0;
}
