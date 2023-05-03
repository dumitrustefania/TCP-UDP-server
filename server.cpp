#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <cmath>

#include "common.h"
#include "helpers.h"

#define MAX_CONNECTIONS 100
#define SERVER_LOCALHOST_IP "127.0.0.1"

char const *data_type_string[4] = {"INT", "SHORT_REAL", "FLOAT", "STRING"};

int subscribed(int fd, char *topic, struct tcp_client tcp_clients[MAX_CONNECTIONS], int num_tcp_clients)
{
    for (int k = 0; k < num_tcp_clients; k++)
    {
        if (tcp_clients[k].fd == fd)
        {
            vector<pair<char *, int>> subscriptions = tcp_clients[k].subscriptions;
            for (int j = 0; j < subscriptions.size(); j++)
                if (!strcmp(subscriptions[j].first, topic))
                    return 1;

            break;
        }
    }
    return 0;
}

struct udp_packet create_recv_packet(char *buf)
{
    struct udp_packet received_packet;
    strcpy(received_packet.topic, buf);
    received_packet.data_type = buf[50];

    if (received_packet.data_type == 0)
    {
        int message = ntohl(*(uint32_t *)(buf + 52));
        if (buf[51] == 1)
            message *= -1;

        sprintf(received_packet.payload, "%d", message);
    }
    else if (received_packet.data_type == 1)
    {
        double message = (double)ntohs(*(uint16_t *)(buf + 51)) / 100.0;
        sprintf(received_packet.payload, "%f", message);
    }
    else if (received_packet.data_type == 2)
    {
        double p = pow(10, (uint8_t)buf[56]);
        double message = (double)ntohl(*(uint32_t *)(buf + 52)) / p;
        if (buf[51] == 1)
            message *= -1;
        sprintf(received_packet.payload, "%f", message);
    }
    else if (received_packet.data_type == 3)
    {
        strcpy(received_packet.payload, buf + 51);
    }

    return received_packet;
}
void run(int listen_tcp, int listen_udp)
{
    struct pollfd poll_fds[MAX_CONNECTIONS];
    struct tcp_client tcp_clients[MAX_CONNECTIONS];
    int num_clients = 3;
    int num_tcp_clients = 0;
    int rc;

    // Setam socket-ul listenfd pentru ascultare
    rc = listen(listen_tcp, MAX_CONNECTIONS);
    DIE(rc < 0, "listen");

    // se adauga noul file descriptor (socketul pe care se asculta conexiuni) in
    // multimea read_fds
    poll_fds[0].fd = listen_tcp;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = listen_udp;
    poll_fds[1].events = POLLIN;

    poll_fds[2].fd = 0;
    poll_fds[2].events = POLLIN;

    while (1)
    {   
        // printf("fac poll...\n");
        rc = poll(poll_fds, num_clients, -1);
        DIE(rc < 0, "poll");

        for (int i = 0; i < num_clients; i++)
        {  
            // printf("fd cu indice=%d\n", i);
            if (poll_fds[i].revents & POLLIN)
            {
                if (poll_fds[i].fd == listen_tcp)
                {
                    // a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
                    // pe care serverul o accepta
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    int newsockfd = accept(listen_tcp, (struct sockaddr *)&cli_addr, &cli_len);
                    DIE(newsockfd < 0, "accept");
                    // se adauga noul socket intors de accept() la multimea descriptorilor
                    // de citire

                    char id[11];
                    // rc = recv_all(newsockfd, &id, sizeof(id));
                    rc = recv(newsockfd, &id, sizeof(id), 0);
                    DIE(rc < 0, "recv");

                    //check for duplicate id...
                    int duplicate = 0;
                    for (int k = 0; k < num_tcp_clients; k++)
                        if (!strcmp(tcp_clients[k].id, id))
                            duplicate = 1;

                    if (!duplicate)
                    {
                        poll_fds[num_clients].fd = newsockfd;
                        poll_fds[num_clients].events = POLLIN;
                        num_clients++;

                        tcp_clients[num_tcp_clients].fd = newsockfd;
                        strcpy(tcp_clients[num_tcp_clients].id, id);
                        num_tcp_clients++;
                        printf("New client %s connected from %s:%d.\n",
                               id, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                    }
                    else
                    {   
                        printf("Client %s already connected.\n", id);

                        char buf[1800];
                        memset(buf, 0, 1800);
                        sprintf(buf, "exit");

                        rc = send_all(newsockfd, &buf, sizeof(buf));
                        DIE(rc < 0, "send");

                        close(newsockfd);
                    }

                    break;
                }
                else if (poll_fds[i].fd == listen_udp)
                {
                    /* Receive a datagram and send an ACK */
                    /* The info of the who sent the datagram (PORT and IP) */
                    struct udp_packet received_packet;
                    struct sockaddr_in client_addr;
                    socklen_t clen = sizeof(client_addr);

                    char buf[1800];
                    memset(buf, 0, 1800);

                    int rc = recvfrom(listen_udp, &buf, 1800, 0,
                                      (struct sockaddr *)&client_addr, &clen);

                    received_packet = create_recv_packet(buf);

                    // now send to subscribed tcp clients....
                    for (int k = 3; k < num_clients; k++)
                    {
                        if (subscribed(poll_fds[k].fd, received_packet.topic, tcp_clients, num_tcp_clients))
                        {
                            memset(buf, 0, 1800);
                            sprintf(buf, "%s:%d - %s - %s - %s", inet_ntoa(client_addr.sin_addr), (ntohs(client_addr.sin_port)),
                                    received_packet.topic, data_type_string[received_packet.data_type], received_packet.payload);

                            rc = send_all(poll_fds[k].fd, &buf, 1800);
                            DIE(rc < 0, "send");
                        }
                    }

                    break;
                }
                else if (poll_fds[i].fd == 0)
                {
                    //citeste exit si inchide server si clienti tcp
                    char buf[1800];
                    memset(buf, 0, 1800);
                    fgets(buf, sizeof(buf), stdin);

                    if (!strncmp(buf, "exit", 4))
                    {
                        for (int k = 0; k < num_tcp_clients; k++)
                        {
                            // daca clientul e conectat atunci inchidem conexiunea si trimitem mesaj sa se inchida si el
                            rc = send_all(tcp_clients[k].fd, &buf, sizeof(buf));
                            DIE(rc < 0, "send");
                            // inchidem si socketii de comunicare cu clientii
                            close(tcp_clients[k].fd);
                        }

                        // oprim server
                        return;
                    }

                    break;
                }
                else
                {
                    // s-au primit date pe unul din socketii de client tcp,
                    // asa ca serverul trebuie sa le receptioneze
                    char buf[1800];
                    memset(buf, 0, 1800);
                    int rc = recv_all(poll_fds[i].fd, &buf, 1800);
                    DIE(rc < 0, "recv");

                    if (rc == 0)
                    {
                        // conexiunea s-a inchis
                        char *id;
                        int client_num;
                        for (int k = 0; k < num_tcp_clients; k++)
                        {
                            if (tcp_clients[k].fd == poll_fds[i].fd)
                            {
                                id = tcp_clients[k].id;
                                client_num = k;
                                break;
                            }
                        }
                        printf("Client %s disconnected.\n", id);
                        close(poll_fds[i].fd);

                        // se scoate din multimea de citire socketul inchis
                        for (int j = i; j < num_clients - 1; j++)
                            poll_fds[j] = poll_fds[j + 1];
                        

                        for (int j = client_num; j < num_tcp_clients - 1; j++)
                            tcp_clients[j] = tcp_clients[j + 1];

                        num_clients--;
                        num_tcp_clients--;
                    }
                    else
                    {
                        char sub[15], topic[50];
                        memset(sub, 0, 15);
                        int sf;
                        sscanf(buf, "%s %s %d", sub, topic, &sf);

                        if (!strcmp(sub, "subscribe"))
                        {
                            for (int k = 0; k < num_tcp_clients; k++)
                            {
                                if (tcp_clients[k].fd == poll_fds[i].fd)
                                {
                                    tcp_clients[k].subscriptions.push_back({strdup(topic), sf});
                                    break;
                                }
                            }

                            memset(buf, 0, 1800);
                            sprintf(buf, "Subscribed to topic.");

                            rc = send_all(poll_fds[i].fd, &buf, 1800);
                            DIE(rc < 0, "send");
                        }
                        else if (!strcmp(sub, "unsubscribe"))
                        {
                            for (int k = 0; k < num_tcp_clients; k++)
                            {
                                if (tcp_clients[k].fd == poll_fds[i].fd)
                                {
                                    vector<pair<char *, int>> subscriptions = tcp_clients[k].subscriptions;
                                    for (int j = 0; j < subscriptions.size(); j++)
                                    {
                                        if (!strcmp(subscriptions[j].first, topic))
                                        {
                                            tcp_clients[k].subscriptions.erase(tcp_clients[k].subscriptions.begin() + j);
                                            break;
                                        }
                                    }
                                }
                            }

                            memset(buf, 0, 1800);
                            sprintf(buf, "Unsubscribed from topic.");

                            rc = send_all(poll_fds[i].fd, &buf, 1800);
                            DIE(rc < 0, "send");
                        }
                    }

                    break;
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc != 2)
    {
        printf("\n Usage: %s <port>\n", argv[0]);
        return 1;
    }

    // Parsam port-ul ca un numar
    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // Obtinem un socket TCP pentru receptionarea conexiunilor
    int listen_tcp = socket(AF_INET, SOCK_STREAM, 0);
    DIE(listen_tcp < 0, "socket_tcp");

    // Obtinem un socket UDP pentru receptionarea conexiunilor
    int listen_udp = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(listen_udp < 0, "socket_udp");

    // Completăm in serv_addr adresa serverului, familia de adrese si portul
    // pentru conectare
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    // Facem adresa socket-ului reutilizabila, ca sa nu primim eroare in caz ca
    // rulam de 2 ori rapid
    int enable = 1;
    if (setsockopt(listen_tcp, IPPROTO_TCP, SO_REUSEADDR | TCP_NODELAY, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) tcp failed");

    if (setsockopt(listen_udp, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) udp failed");

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    rc = inet_pton(AF_INET, SERVER_LOCALHOST_IP, &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    // Asociem adresa serverului cu socketul creat folosind bind
    rc = bind(listen_tcp, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind tcp");

    rc = bind(listen_udp, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind udp");

    run(listen_tcp, listen_udp);

    // Inchidem listenfd
    close(listen_tcp);
    close(listen_udp);

    return 0;
}
