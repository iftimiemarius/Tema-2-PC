#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <dirent.h>

#include "utils.h"

void usage(char *file) {
	fprintf(stderr, "Usage: %s server_port\n \
               * <server_port> must be between than 1024 and %d\n",
               file, USHRT_MAX);
	exit(0);
}

void udp_to_tcp(char *ip, uint16_t port,
                udp_packet *current_udp_packet,
                tcp_packet *current_tcp_packet) {

  char *current_message = current_tcp_packet->message;
  // Check type
  if (current_udp_packet->data_type == INT_TYPE) {

    // First byte represents the sign of the integer
    char negative[NEGATIVE_LENGTH] = " ";
    if (*((uint8_t*) current_udp_packet->content)) {
      negative[1] = '-';
    }

    // Next 4 bytes contain the integer itself in network byte order
    char *new_position = current_udp_packet->content + 1;
    uint32_t integer = ntohl(*((uint32_t*) new_position));

    // Save the final message
    sprintf(current_message, "%s:%d - %s - INT -%s%d\n",
                             ip,
                             port,
                             current_udp_packet->topic,
                             negative,
                             integer);

  } else if (current_udp_packet->data_type == SHORT_REAL_TYPE) {
    // 2 bytes contain the module of a float with 2 decimals
    uint16_t short_real = ntohs(*((uint16_t*) current_udp_packet->content));
    float final_number = short_real * pow(10, -2);

    // Save the final message
    sprintf(current_message, "%s:%d - %s - SHORT_REAL - %g\n",
                             ip,
                             port,
                             current_udp_packet->topic,
                             final_number);

  } else if (current_udp_packet->data_type == FLOAT_TYPE) {
    // First byte represents the sign of the real number
    char negative[NEGATIVE_LENGTH] = " ";
    if (*((uint8_t*) current_udp_packet->content)) {
      negative[1] = '-';
    }

    // Next 4 bytes contain the number itself in network byte order
    char *new_position = current_udp_packet->content + 1;
    uint32_t integer = ntohl(*((uint32_t*) new_position));

    // Next byte contains the number of digits of the fractional part
    new_position = current_udp_packet->content + FLOAT_OFF;
    uint8_t decimals = *((uint8_t*) new_position);
    float final_number = integer * pow(10, -decimals);

    // Save the final message
    sprintf(current_message, "%s:%d - %s - FLOAT -%s%f\n",
                             ip,
                             port,
                             current_udp_packet->topic,
                             negative,
                             final_number);

  } else if (current_udp_packet->data_type == STRING_TYPE) {
    sprintf(current_message, "%s:%d - %s - STRING - %s\n",
                             ip,
                             port,
                             current_udp_packet->topic,
                             current_udp_packet->content);
  }
}

void process_udp_packet(int sock_fd, udp_packet *current_udp_packet,
      tcp_packet *current_tcp_packet, struct sockaddr_in client_address,
      socklen_t client_length, topic_node **topics, int *online_clients) {

  int result;

  // Receive the message
  memset(current_udp_packet, 0, sizeof(udp_packet));
         client_length = sizeof(struct sockaddr_in);
  result = recvfrom(sock_fd, current_udp_packet, sizeof(udp_packet), 0,
                    (struct sockaddr*) &client_address,
                    &client_length);
  DIE(result < 0, "recvfrom");

  topic_node *current_topic = find_topic(*topics,
                                         current_udp_packet->topic);
          
  if (current_topic == NULL) {
    *topics = add_topic(*topics, current_udp_packet->topic);
    current_topic = *topics; // The new topic is at the front of the list
  }

  char *client_ip = inet_ntoa(client_address.sin_addr);
  uint16_t client_port = ntohs(client_address.sin_port);

  udp_to_tcp(client_ip, client_port,
             current_udp_packet, current_tcp_packet);

  client_node *current_clients = current_topic->clients;
  while (current_clients != NULL) {
    // Check if the client is online
    if (online_clients[current_clients->sock_fd]) {

      int client_socket = online_clients[current_clients->sock_fd];
      result = send(client_socket, current_tcp_packet,
                    strlen(current_tcp_packet->message), 0);
      DIE(result < 0, "send");

      current_clients->unsent_messages = 0;
    } else {
      // Save the message if the store and forward flag is active
      if (current_clients->sf) {
        // Create file that contains the message
        char current_file[FILE_LENGTH];
        sprintf(current_file, "%s/%s/%s_%d.msg",
                UNSENT_MESSAGES,
                current_clients->id,
                current_topic->name,
                current_clients->unsent_messages);
        // Add the message to the file
        FILE *file = fopen(current_file, "wb");
        DIE(file == NULL, "fopen");
        fwrite(current_tcp_packet,
               strlen(current_tcp_packet->message),
               1,
               file);
        fclose(file);
        ++(current_clients->unsent_messages);
      }
    }
    current_clients = current_clients->next;
  }
}

void unsent_messages(char *current_directory_path,
                     tcp_packet *current_tcp_packet, int sock_fd) {

  int result;
  DIR *current_directory = opendir(current_directory_path);
  if (current_directory == NULL) {
    result = mkdir(current_directory_path, DIRECTORY_PERMISSIONS);
    DIE(result == -1, "mkdir");
  } else {
    // Send remaining messages
    struct dirent *de;

    while ((de = readdir(current_directory))) {
      // Ignore . and ..
      if (strcmp(de->d_name, CURRENT_DIR) && strcmp(de->d_name, PREVIOUS_DIR)) {
        char message_path[MESSAGE_PATH_LENGTH];
        // Get the path for the message
        sprintf(message_path, "%s/%s", current_directory_path, de->d_name);

        // Initialize the current packet with 0
        memset(current_tcp_packet, 0, sizeof(tcp_packet));

        // Read the message in the current packet
        FILE *file = fopen(message_path, "rb");
        DIE(file == NULL, "fopen");

        fseek(file, 0, SEEK_END);
        int size = ftell(file);
        fseek(file, 0, SEEK_SET);

        fread(current_tcp_packet, 1, size, file);
        fclose(file);

        // Delete the file containing the message
        rm(message_path);

        // Send the message to the client
        result = send(sock_fd, current_tcp_packet,
                      strlen(current_tcp_packet->message), 0);
        DIE(result < 0, "send");
      }
    }
    closedir(current_directory);
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2 || atoi(argv[1]) < 1024 || atoi(argv[1]) > USHRT_MAX) {
    usage(argv[0]);
  }

  int result;

  unsigned int port = atoi(argv[1]);

  // Initialize server structure
  struct sockaddr_in server_address = { .sin_family = AF_INET,
                                        .sin_port = htons(port),
                                        .sin_addr = { .s_addr = INADDR_ANY } };

  int udp_sock_fd;
  int tcp_sock_fd;

  char buffer[BUFLEN];
  udp_packet *current_udp_packet = malloc (sizeof(udp_packet));
  DIE(current_udp_packet == NULL, "malloc");

  tcp_packet *current_tcp_packet = malloc (sizeof(tcp_packet));
  DIE(current_tcp_packet == NULL, "malloc");

  struct sockaddr_in client_address;
  socklen_t client_length;

  fd_set read_fd;
  fd_set current_fd;
  int fd_max;

  FD_ZERO(&read_fd);

  // Open the sockets
  udp_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
  tcp_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

  // Disable Nagle's algorithm
  uint32_t flag = 1;
  result = setsockopt(tcp_sock_fd,
                      IPPROTO_TCP,
                      TCP_NODELAY,
                      &flag,
                      sizeof(uint32_t));
  DIE(result < 0, "setsockopt");

  // Bind UDP and TCP sockets
  result = bind(udp_sock_fd, (struct sockaddr*) &server_address,
                sizeof(struct sockaddr));
  DIE(result < 0, "bind udp");

  result = bind(tcp_sock_fd, (struct sockaddr*) &server_address,
                sizeof(struct sockaddr));
  DIE(result < 0, "bind tcp");

  // Listen to the TCP socket
  result = listen(tcp_sock_fd, MAX_CLIENTS);
  DIE(result < 0, "listen tcp");

  // Add the stdin, udp and tcp ports
  FD_SET(STDIN_FILENO, &read_fd);
  FD_SET(udp_sock_fd, &read_fd);
  FD_SET(tcp_sock_fd, &read_fd);
  fd_max = tcp_sock_fd + 1;

  // Initialize online_clients, initialized_clients
  int clients_number = 0;
  int clients_size = MAX_CLIENTS;

  int *online_clients = calloc (clients_size, sizeof(int));
  DIE(online_clients == NULL, "online_clients alloc");

  // Initialize ids
  int current_ids = fd_max;
  char **ids = malloc (current_ids * sizeof(char*));
  DIE(ids == NULL, "ids alloc");
  for (int i = 0; i < current_ids; i++) {
    ids[i] = calloc (ID_LENGTH, sizeof(int));
    DIE(ids == NULL, "ids alloc");
  }

  // Initialize with no topics
  topic_node *topics = NULL;

  // Delete old directory and create a new one to store unsent messages
  rm(UNSENT_MESSAGES);
  result = mkdir(UNSENT_MESSAGES, DIRECTORY_PERMISSIONS);
  DIE(result == -1, "mkdir");

  // The server will stop running if "exit" is written at console input (stdin)
  int running = 1;
  while (running) {
    current_fd = read_fd;

    result = select(fd_max, &current_fd, NULL, NULL, NULL);
    DIE(result < 0, "select");

    for (int i = 0; i < fd_max; i++) {
      if (FD_ISSET(i, &current_fd)) {
        if (i == STDIN_FILENO) {
          memset(buffer, 0, BUFLEN);
          fgets(buffer, BUFLEN, stdin);
          if (strstr(buffer, "exit")) {
            running = 0;
            for (int j = udp_sock_fd; j < fd_max; j++) {
              close(j);
            }
            break;
          }
        } else if (i == udp_sock_fd) {
          // Message received from UDP socket
          process_udp_packet(i, current_udp_packet, current_tcp_packet,
                client_address, client_length, &topics, online_clients);
          
        } else if (i == tcp_sock_fd) {
          // Accept a new connection
          client_length = sizeof(struct sockaddr_in);
          int latest_sock_fd = accept(i, (struct sockaddr*) &client_address,
                                       &client_length);
          DIE(latest_sock_fd < 0, "accept");


          // Disable Nagle's algorithm
          flag = 1;
          result = setsockopt(latest_sock_fd,
                              IPPROTO_TCP,
                              TCP_NODELAY,
                              &flag,
                              sizeof(uint32_t));
          DIE(result < 0, "setsockopt");

          // Add the latest socket
          FD_SET(latest_sock_fd, &read_fd);
          
          // Update file descriptor max value
          fd_max = max(fd_max, latest_sock_fd + 1);
          
          // Reallocate the ids array if needed
          while (fd_max >= current_ids) {
            char **tmp = realloc(ids,
                                 2 * current_ids * sizeof(char[ID_LENGTH]));
            DIE(tmp == NULL, "ids realloc");
            ids = tmp;
            for (int j = current_ids; j < 2 * current_ids; j++) {
              ids[j] = calloc(ID_LENGTH, sizeof(char));
              DIE(ids[j] == NULL, "ids realloc");
            }
            current_ids *= 2;
          }

          // Get the client_id
          memset(current_tcp_packet, 0, sizeof(tcp_packet));
          result = recv(latest_sock_fd, current_tcp_packet,
                        sizeof(tcp_packet), 0);
          DIE(result < 0, "recv");

          memcpy(ids[latest_sock_fd], current_tcp_packet->message, ID_LENGTH);

          // Reallocate the online_clients and initialized_clients if needed
          while (fd_max >= clients_size) {
            int *tmp = realloc(online_clients, 2 * clients_size * sizeof(int));
            DIE(tmp == NULL, "online_clients realloc");
            online_clients = tmp;
            memset(online_clients + clients_size, 0, clients_size);

            clients_size *= 2;
          }

          printf("New client (%s) connected from %s:%d.\n",
                 ids[latest_sock_fd],
                 inet_ntoa(client_address.sin_addr),
                 ntohs(client_address.sin_port));

          // Go to directory and send remaining messages
          char current_directory[DIRECTORY_LENGTH];
          sprintf(current_directory, "%s/%s", UNSENT_MESSAGES, ids[latest_sock_fd]);

          unsent_messages(current_directory, current_tcp_packet, latest_sock_fd);

          online_clients[latest_sock_fd] = latest_sock_fd;
          clients_number++;
        } else {
          // Data received from a TCP client
          memset(current_tcp_packet, 0, sizeof(tcp_packet));
          result = recv(i, current_tcp_packet, sizeof(tcp_packet), 0);
          DIE(result < 0, "recv");

          if (result == 0) {
            // Connection closed
            printf("Client (%s) has disconnected.\n", ids[i]);
            online_clients[i] = 0;
            close(i);
            FD_CLR(i, &read_fd);
          } else {
            char command[COMMAND_LENGTH];
            char topic_name[TOPIC_LENGTH];
            int sf;

            parse_command(current_tcp_packet->message, command, topic_name, &sf);

            topic_node *current_topic = find_topic(topics, topic_name);
            if (current_topic == NULL) {
              topics = add_topic(topics, topic_name);  // Add the new topic

              // The new list head represents the new topic
              current_topic = topics;
            }
            if (!strcmp(command, SUBSCRIBE)) {
              if (!find_client(current_topic->clients, ids[i])) {
                // Add the client to the topic if it does not already exist
                current_topic->clients = add_client(current_topic->clients,
                                                    ids[i], i, sf);
              }
            } else if (!strcmp(command, UNSUBSCRIBE)) {
              current_topic->clients = delete_client(current_topic->clients,
                                                     ids[i]);

            }
          }
        }
      }
    }
  }

  // Free the memory used
  destroy_topics(topics);

  free(current_udp_packet);
  free(current_tcp_packet);
  
  for (int i = 0; i < current_ids; i++) {
    free(ids[i]);
  }
  free(ids);
  free(online_clients);

  // Delete the directory containing unsent messages
  result = rm(UNSENT_MESSAGES);

	return 0;
}
