#ifndef UTILS_H
#define UTILS_H

#define _XOPEN_SOURCE 500
#define __USE_XOPEN_EXTENDED 1

#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ftw.h>
#include <fts.h>

#define DIE(assertion, call_description)  \
  do {                  \
    if (assertion) {          \
      fprintf(stderr, "(%s, %d): ", \
          __FILE__, __LINE__);  \
      perror(call_description);   \
      exit(EXIT_FAILURE);       \
    }                 \
  } while(0)

#define SPACE_NEWLINE " \n"
#define COMMAND_COUNTER 0
#define TOPIC_COUNTER 1
#define SF_COUNTER 2
#define BUFLEN 256
#define MAX_CLIENTS 100
#define MAX_TCP_PACKETS 100
#define TOPIC_LENGTH 50
#define CONTENT_LENGTH 1500
#define TCP_LENGTH 1600
#define COMMAND_LENGTH 20
#define ID_LENGTH 11
#define IP_LENGTH 20
#define INT_TYPE 0
#define SHORT_REAL_TYPE 1
#define FLOAT_TYPE 2
#define STRING_TYPE 3
#define FLOAT_OFF 5
#define NEGATIVE_LENGTH 3
#define SUBSCRIBE "subscribe"
#define UNSUBSCRIBE "unsubscribe"
#define UNSENT_MESSAGES "unsent"
#define DIRECTORY_PERMISSIONS 0777
#define DIRECTORY_LENGTH 50
#define FILE_LENGTH 100
#define MESSAGE_PATH_LENGTH 300
#define CURRENT_DIR "."
#define PREVIOUS_DIR ".."

typedef struct topic topic_node;
typedef struct client client_node;

#pragma pack(1)

typedef struct {
  char topic[TOPIC_LENGTH];
  uint8_t data_type;
  char content[CONTENT_LENGTH];
} udp_packet;

typedef struct {
  char message[TCP_LENGTH];
} tcp_packet;

struct client {
  char id[ID_LENGTH];
  uint32_t sock_fd;
  uint32_t sf;
  uint32_t unsent_messages;
  client_node *next;
};

struct topic {
  char name[TOPIC_LENGTH];
  client_node *clients;
  topic_node *next;
};

int max(int a, int b);

int is_number(char *id);

int parse_command(char *buffer, char *command, char *topic, int *sf);

topic_node* add_topic(topic_node *head, char *name);
topic_node* find_topic(topic_node *head, char *name);
void update_sockets(topic_node *head, char *id, uint32_t sock_fd);
void destroy_topics(topic_node *head);
client_node* add_client(client_node *head, char *id, uint32_t sock_fd, uint32_t sf);
client_node* delete_client(client_node *head, char *id);
client_node* find_client(client_node *head, char *id);
void update_socket(client_node *head, char *id, uint32_t sock_fd);
void destroy_clients(client_node *head);

unsigned int get_index(char **ids, char *id, int current_ids);

int fn(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
int rm(char *path);
int compare_fts(const FTSENT **first, const FTSENT **second);

#endif
