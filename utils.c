#define _XOPEN_SOURCE 500
#define __USE_XOPEN_EXTENDED 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ftw.h>
#include <ctype.h>

#include "utils.h"


int max(int a, int b) {
  return (a > b) ? a : b;
}

int is_number(char *id) {
  for (int i = 0; i < strlen(id); i++) {
    if (!isdigit(id[i])) {
      return 0;
    }
  }
  return 1;
}

int parse_command(char *buffer, char *command, char *topic, int *sf) {
  *sf = 0;
  char *aux_buffer = strdup(buffer);
  int counter = 0;
  char *rest = NULL;
  char *token = strtok_r(aux_buffer, SPACE_NEWLINE, &rest);
  int correct = 1;
  while (token != NULL) {
    if (counter == COMMAND_COUNTER) {
      if (strlen(token) > COMMAND_LENGTH) {
        // the command must not exceed 20 bytes
        correct = 0;
      } else if (strcmp(token, SUBSCRIBE) && strcmp(token, UNSUBSCRIBE)) {
        // the command must be either <subscribe> or <unsubscribe>
        correct = 0;
      }
      memcpy(command, token, COMMAND_LENGTH);
    } else if (counter == TOPIC_COUNTER) {
      if (strlen(command) > TOPIC_LENGTH) {
        // the topic must not exceed 50 bytes
        correct = 0;
      }
      memcpy(topic, token, TOPIC_LENGTH);
    } else if (counter == SF_COUNTER && !strcmp(command, SUBSCRIBE)) {
      *sf = atoi(token);
      if (*sf != 0 && *sf != 1) {
        // sf must be either 0 or 1
        correct = 0;
      }
    } else {
      // Wrong usage of command
      correct = 0;
    }
    counter++;
    token = strtok_r(NULL, SPACE_NEWLINE, &rest);
  }
  if (counter == SF_COUNTER && !strcmp(command, SUBSCRIBE)) {
    // subscribe command must be proceeded by sf flag
    correct = 0;
  }
  free(aux_buffer);
  return correct;
}


topic_node* add_topic(topic_node *head, char *name) {
  topic_node *new_node = malloc (sizeof(topic_node));
  DIE(new_node == NULL, "add topic");
  new_node->next = head;
  new_node->clients = NULL;
  memcpy(new_node->name, name, TOPIC_LENGTH);
  return new_node;
}

topic_node* find_topic(topic_node *head, char *name) {
  while (head != NULL) {
    if (!strcmp(head->name, name)) {
      return head;
    }
    head = head->next;
  }
  return NULL;
}

void update_sockets(topic_node *head, char *id, uint32_t sock_fd) {
  while (head != NULL) {
    update_socket(head->clients, id, sock_fd);
    head = head->next;
  }
}

void destroy_topics(topic_node *head) {
  while (head != NULL) {
    topic_node *current = head;
    head = head->next;
    destroy_clients(current->clients);
    free(current);
  }
}

client_node* add_client(client_node *head, char *id, uint32_t sock_fd,
                        uint32_t sf) {
  client_node *new_node = malloc (sizeof(client_node));
  DIE(new_node == NULL, "add client");
  memcpy(new_node->id, id, ID_LENGTH);
  new_node->sock_fd = sock_fd;
  new_node->sf = sf;
  new_node->unsent_messages = 0;
  new_node->next = head;
  return new_node;
}

client_node* delete_client(client_node *head, char *id) {
  client_node *current = head;
  client_node *prev = NULL;
  while (current != NULL) {
    if (!strcmp(current->id, id)) {
      break;
    }
    prev = current;
    current = current->next;
  }
  if (current) {
    if (prev) {
      prev->next = current->next;
    } else {
      head = head->next;
    }
    free(current);
  }
  return head;
}

client_node* find_client(client_node *head, char *id) {
  while (head != NULL) {
    if (!strcmp(head->id, id)) {
      return head;
    }
    head = head->next;
  }
  return NULL;
}

void update_socket(client_node *head, char *id, uint32_t sock_fd) {
  while (head != NULL) {
    if (!strcmp(head->id, id)) {
      head->sock_fd = sock_fd;
    }
  }
}

void destroy_clients(client_node *head) {
  while (head != NULL) {
    client_node *current = head;
    head = head->next;
    free(current);
  }
}

unsigned int get_index(char **ids, char *id, int current_ids) {
  for (unsigned int i = 0; i < current_ids; i++) {
    if (!strcmp(ids[i], id)) {
      return i;
    }
  }
  return -1;
}

int fn(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
  int result = remove(fpath);
  DIE(result != 0, "remove");
  return result;
}

int rm(char *path) {
  return nftw(path, fn, FTW_D, FTW_DEPTH | FTW_PHYS);
}

int compare_fts(const FTSENT **first, const FTSENT **second) {
  return strcmp((*first)->fts_name, (*second)->fts_name);
}