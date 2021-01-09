#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>

#include "utils.h"

void usage(char *file) {
	fprintf(stderr, "Usage: %s client_id server_address server_port\n \
                   * <client_id> must be a string of maximum %d characters\n \
                   * <server_port> must be between 1024 and %d\n",
                   file, ID_LENGTH - 1, USHRT_MAX);
	exit(0);
}

int main(int argc, char *argv[]) {
  if (argc != 4 || atoi(argv[3]) < 1024 || strlen(argv[1]) > ID_LENGTH - 1) {
    usage(argv[0]);
  }

  int result;

  unsigned int port = atoi(argv[3]);
  struct in_addr ip;
  result = inet_aton(argv[2], &ip);
  DIE(result == 0, "inet_aton");

  // Initialize server structure
  struct sockaddr_in server_address = { .sin_family = AF_INET,
                                        .sin_port = htons(port),
                                        .sin_addr = { .s_addr = ip.s_addr } };

  int tcp_sock_fd;

  tcp_packet *current_tcp_packet = malloc(sizeof(tcp_packet));

  fd_set read_fd;
  fd_set current_fd;
  int fd_max;

  FD_ZERO(&read_fd);

  // Open the socket
  tcp_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(tcp_sock_fd < 0, "socket");

  // Disable Nagle's algorithm
  uint32_t flag = 1;
  result = setsockopt(tcp_sock_fd,
                      IPPROTO_TCP,
                      TCP_NODELAY,
                      &flag,
                      sizeof(uint32_t));
  DIE(result < 0, "setsockopt");

  // Add the console input - stdin and the tcp socket
  FD_SET(STDIN_FILENO, &read_fd);
  FD_SET(tcp_sock_fd, &read_fd);
  fd_max = tcp_sock_fd + 1;

  // Connect to the server
  result = connect(tcp_sock_fd, (struct sockaddr*) &server_address,
                   sizeof(struct sockaddr_in));
  DIE(result < 0, "connect");

  memset(current_tcp_packet, 0, sizeof(tcp_packet));
  memcpy(current_tcp_packet->message, argv[1], strlen(argv[1]));

  result = send(tcp_sock_fd, current_tcp_packet, sizeof(tcp_packet), 0);
  DIE(result < 0, "send");

  int running = 1;
  while (running) {
    current_fd = read_fd;
    result = select(fd_max, &current_fd, NULL, NULL, NULL);
    DIE(result < 0, "select");

    if (FD_ISSET(0, &current_fd)) {
      memset(current_tcp_packet, 0, sizeof(tcp_packet));
      fgets(current_tcp_packet->message, CONTENT_LENGTH, stdin);

      if (strstr(current_tcp_packet->message, "exit")) {
        running = 0;
        break;
      }

      char command[COMMAND_LENGTH] = { 0 };
      char topic[TOPIC_LENGTH] = { 0 };
      int sf;

      // Check if the command is valid
      if (!parse_command(current_tcp_packet->message, command, topic, &sf)) {
        continue;
      }
      
      result = send(tcp_sock_fd, current_tcp_packet, sizeof(tcp_packet), 0);
      DIE(result < 0, "send");

      printf("%sd %s\n", command, topic);

      // Sending message to server
    } else if (FD_ISSET(tcp_sock_fd, &current_fd)) {
      // Message received from server
      memset(current_tcp_packet, 0, sizeof(tcp_packet));

      result = recv(tcp_sock_fd, current_tcp_packet, sizeof(tcp_packet), 0);
      DIE(result < 0, "recv");
      
      if (result == 0) {
        running = 0;
        break;
      }

      // Printing the information from the subscribed topic
      printf("%s", current_tcp_packet->message);
    }
  }

  free(current_tcp_packet);
  close(tcp_sock_fd);

  return 0;
}