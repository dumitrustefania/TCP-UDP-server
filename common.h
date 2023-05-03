#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <string>

using namespace std;

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

/* Dimensiunea maxima a mesajului */
#define PAYLOAD_MAXSIZE 1500
#define TOPIC_MAXSIZE 50

struct udp_packet {
  char topic[TOPIC_MAXSIZE + 1];
  unsigned int data_type;
  char payload[PAYLOAD_MAXSIZE + 1];
};

struct tcp_client {
  char id[11];
  int fd;
  vector<pair<char*, int>> subscriptions;
  int connected;
};

#endif
