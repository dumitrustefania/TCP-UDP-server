#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>

/*
 * Macro de verificare a erorilor
 */

#define DIE(assertion, call_description)                                       \
  do {                                                                         \
    if (assertion) {                                                           \
      fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
      perror(call_description);                                                \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

#define MAX_CONNECTIONS 1000
#define MAX_BUF_SIZE 1600

#define SERVER_LOCALHOST_IP "127.0.0.1"

/*
  Pachetul pe care clientii UDP il trimit serverului.
  Contine topic, tipul de date si datele.
*/
struct udp_packet {
  char topic[51];
  unsigned int data_type;
  char payload[1501];
};

/*
  Structura de date ce retine clientii TCP care sunt sau
  au fost conectati la server, si toate datele de care avem nevoie
  despre acestia.
*/
struct tcp_client {
  char id[11];
  int fd;
  vector<pair<char*, int>> subscriptions;
  int connected;
};
#endif
