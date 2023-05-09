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
#include <unordered_map>
#include <queue>

#include "common.h"
#include "helpers.h"

// Codificarea celor 4 data types sub forma de string
char const *data_type_string[4] = {"INT", "SHORT_REAL", "FLOAT", "STRING"};

// Verific daca un topic se afla in lista de abonari ale unui client
// Daca acesta nu e abonat, retunez 0.
// Daca e abonat fara store-forward, returnez 1.
// Daca doreste si store-forward, returnez 2.
int check_subscribed(vector<pair<char *, int>> &subscriptions, char *topic)
{
    for (int j = 0; j < subscriptions.size(); j++)
        if (!strcmp(subscriptions[j].first, topic))
            return 1 + subscriptions[j].second;

    return 0;
}

// Creez pachetul de tip 'udp_packet' din bufferul citit de pe socketul
// corespunzator conexiunilor UDP
struct udp_packet create_recv_packet(char *buf)
{
    struct udp_packet received_packet;

    // Copiez topicul
    strcpy(received_packet.topic, buf);
    // Memorez data_type, un unsigned int ce poate lua
    // valori de la 0 la 4
    received_packet.data_type = buf[50];

    // In functie de data type, parsez payload-ul si il
    // retin ca string
    if (received_packet.data_type == 0) //INT
    {
        int message = ntohl(*(uint32_t *)(buf + 52));

        if (buf[51] == 1) // octetul de semn e 1 -> negativ
            message *= -1;

        sprintf(received_packet.payload, "%d", message);
    }
    else if (received_packet.data_type == 1) // SHORT_REAL
    {
        double message = (double)ntohs(*(uint16_t *)(buf + 51)) / 100.0;
        sprintf(received_packet.payload, "%f", message);
    }
    else if (received_packet.data_type == 2) // FLOAT
    {
        double p = pow(10, (uint8_t)buf[56]);
        double message = (double)ntohl(*(uint32_t *)(buf + 52)) / p;

        if (buf[51] == 1) // octetul de semn e 1 -> negativ
            message *= -1;

        sprintf(received_packet.payload, "%f", message);
    }
    else if (received_packet.data_type == 3) // STRING
    {
        strcpy(received_packet.payload, buf + 51);
    }
    else
    {
        fprintf(stderr, "Unrecognized data type\n");
    }

    return received_packet;
}

// Functionalitatea principala a serverului,
// care asteapta pachete de la diversi fd:
// stdin, socketii de listen de conexiuni TCP sau UDP
// si socketii TCP
void run(int listen_tcp, int listen_udp)
{
    int rc;
    char buf[MAX_BUF_SIZE];

    // Mentinem un vector cu toti clientii TCP care sunt sau
    // au fost conectati la server.
    struct tcp_client tcp_clients[MAX_CONNECTIONS];
    int num_tcp_clients = 0;

    // Fiecare pachet deconectat va mentine cate o coada de pachete
    // in caz ca acestea sunt primite si el avea store-forward activat
    // pentru ele. Aceste pachete sunt apoi trimise la reconectare.
    unordered_map<string, queue<string>> packets_queue;

    // Setam socket-ul listen_tcp pentru ascultare
    rc = listen(listen_tcp, MAX_CONNECTIONS);
    DIE(rc < 0, "listen");

    // Cream multimea de file descriptori monitorizati.
    // Pentru inceput, acestia sunt 3: stdin, listen_tcp
    // si listen_udp.
    struct pollfd poll_fds[MAX_CONNECTIONS];
    int num_clients = 3;

    poll_fds[0].fd = listen_tcp;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = listen_udp;
    poll_fds[1].events = POLLIN;

    poll_fds[2].fd = 0;
    poll_fds[2].events = POLLIN;

    while (1)
    {
        // Facem poll pe file descriptori pana cand se poate
        // citi informatie de pe unul dintre ei
        rc = poll(poll_fds, num_clients, -1);
        DIE(rc < 0, "poll");

        for (int i = 0; i < num_clients; i++)
        {
            if (poll_fds[i].revents & POLLIN)
            {
                // Daca primim o cerere de conectare de la un client TCP
                if (poll_fds[i].fd == listen_tcp)
                {
                    // Serverul accepta conexiunea
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    int newsockfd = accept(listen_tcp, (struct sockaddr *)&cli_addr, &cli_len);
                    DIE(newsockfd < 0, "accept");

                    // Imediat dupa cererea de conectare, clientul isi trimite ID-ul,
                    // asa ca il primim si pe acesta.
                    char id[11];
                    rc = recv(newsockfd, &id, sizeof(id), 0);
                    DIE(rc < 0, "recv");

                    // Verificam statusul clientului TCP
                    // 0 - nu a fost conectat pana acum
                    // 1 - a fost conectat, doreste reconectare
                    // 2 - e deja conectat, ilegal!
                    int status = 0;
                    int tcp_pos = -1;
                    for (int k = 0; k < num_tcp_clients; k++)
                        if (!strcmp(tcp_clients[k].id, id))
                        {
                            tcp_pos = k;
                            status = 1;
                            if (tcp_clients[k].connected == 1)
                                status = 2;
                            break;
                        }

                    // Daca nu e deja conectat
                    if (status < 2)
                    {
                        // Adaugam noul file descriptor in lista de poll
                        poll_fds[num_clients].fd = newsockfd;
                        poll_fds[num_clients].events = POLLIN;
                        num_clients++;

                        printf("New client %s connected from %s:%d.\n",
                               id, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

                        // Daca este la prima conexiune, adaugam noul client TCP si in lista
                        // interna de clienti TCP
                        if (status == 0)
                        {
                            tcp_clients[num_tcp_clients].fd = newsockfd;
                            tcp_clients[num_tcp_clients].connected = 1;
                            strcpy(tcp_clients[num_tcp_clients].id, id);
                            num_tcp_clients++;
                        }
                        else // Daca se reconecteaza
                        {
                            // Updatam cu noul file descriptor si memoram ca s-a conectat
                            tcp_clients[tcp_pos].connected = 1;
                            tcp_clients[tcp_pos].fd = newsockfd;

                            // Trimitem din coada toate pachetele care s-au trimis in timp
                            // ce clientul era deconectat, la care acesta era abonat si avea
                            // store-forward activat.
                            while (!packets_queue[id].empty())
                            {
                                rc = send_all(newsockfd, &packets_queue[id].front()[0], MAX_BUF_SIZE);
                                DIE(rc < 0, "send");
                                packets_queue[id].pop();
                            }
                        }
                    }
                    else // Daca clientul e deja conectat cu acelasi ID, actiunea e ilegala
                    {
                        printf("Client %s already connected.\n", id);

                        // Trimitem cerere de inchidere catre client
                        memset(buf, 0, MAX_BUF_SIZE);
                        sprintf(buf, "exit");
                        rc = send_all(newsockfd, &buf, sizeof(buf));
                        DIE(rc < 0, "send");

                        // Inchidem socketul
                        close(newsockfd);
                    }

                    break;
                }
                // Daca primim un pachet de la un client UDP
                else if (poll_fds[i].fd == listen_udp)
                {
                    struct sockaddr_in client_addr;
                    socklen_t clen = sizeof(client_addr);
                    memset(buf, 0, MAX_BUF_SIZE);

                    // Receptionam pachetul trimis de clientul UDP
                    int rc = recvfrom(listen_udp, &buf, MAX_BUF_SIZE, 0,
                                      (struct sockaddr *)&client_addr, &clen);

                    // Convertim pachetul primit sub forma de char* intr-o structura de tipul udp_packet
                    struct udp_packet received_packet = create_recv_packet(buf);

                    // Trimitem pachetul clientilor TCP abonati la topicul primit
                    for (int k = 0; k < num_tcp_clients; k++)
                    { // Verificam daca clientul TCP k este abonat la topic
                        // sub = 0 - nu e abonat
                        // sub = 1 - e abonat cu sf = 0
                        // sub = 2 - e abonat cu sf = 1
                        int sub = check_subscribed(tcp_clients[k].subscriptions, received_packet.topic);

                        if (sub > 0) // daca e abonat
                        {
                            // transformam informatiile primite stringul ce va fi trimis clientului TCP
                            memset(buf, 0, MAX_BUF_SIZE);
                            sprintf(buf, "%s:%d - %s - %s - %s\n", inet_ntoa(client_addr.sin_addr),
                                    (ntohs(client_addr.sin_port)), received_packet.topic,
                                    data_type_string[received_packet.data_type], received_packet.payload);

                            // Daca clientul e conectat, ii trimitem direct stringul
                            if (tcp_clients[k].connected)
                            {
                                rc = send_all(tcp_clients[k].fd, &buf, sizeof(buf));
                                DIE(rc < 0, "send");
                            }
                            // Daca nu e conectat dar are sf = 1 pe topicul curent, stocam stringul
                            //  in coada de mesaje a clientului in caz ca se va reconecta mai tarziu
                            else if (sub == 2)
                                packets_queue[tcp_clients[k].id].push(buf);
                        }
                    }

                    break;
                }
                // Daca primim un mesaj de la stdin
                else if (poll_fds[i].fd == 0)
                {
                    // Citim mesajul
                    memset(buf, 0, MAX_BUF_SIZE);
                    fgets(buf, sizeof(buf), stdin);

                    // Daca mesajul curent este 'exit'
                    if (!strncmp(buf, "exit", 4))
                    {
                        // Parcurgem toti clientii TCP
                        for (int k = 0; k < num_tcp_clients; k++)
                        {
                            // Daca clientul e conectat, ii trimitem mesajul 'exit', ca sa se inchida
                            // si inchidem si noi socketul de conexiune intre el si server
                            if (tcp_clients[k].connected)
                            {
                                rc = send_all(tcp_clients[k].fd, &buf, sizeof(buf));
                                DIE(rc < 0, "send");
                                close(tcp_clients[k].fd);
                            }
                        }

                        // Revenim in main, care inchide socketii de listen si opreste serverul
                        return;
                    }
                    else
                        fprintf(stderr, "Unrecognized command.\n");
                    
                    break;
                }
                // Daca se primesc date de pe socketul unuia dintre clientii TCP conectati
                else
                {
                    // Receptionam mesajul ca string
                    memset(buf, 0, MAX_BUF_SIZE);
                    int rc = recv_all(poll_fds[i].fd, &buf, MAX_BUF_SIZE);
                    DIE(rc < 0, "recv");

                    // Clientul TCP a inchis conexiunea
                    if (rc == 0)
                    {
                        // Determinam ID ul clientului TCP, caruia ii stim doar fd-ul socketului
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

                        // Inchidem socketul corespunzator
                        close(poll_fds[i].fd);

                        // Scoatem socketul din vectorul de poll
                        for (int j = i; j < num_clients - 1; j++)
                            poll_fds[j] = poll_fds[j + 1];
                        num_clients--;

                        // Marcam clientul TCP in vectorul de clienti ca fiind deconectat
                        tcp_clients[client_num].connected = 0;
                        tcp_clients[client_num].fd = -1;
                    }
                    else // Clientul este inca conectat
                    {
                        // Parsam stringul primit si extragem cele 3 componente:
                        // - subscribe/unsubscribe
                        // - topicul
                        // - store-forward = 0/1
                        char sub[15], topic[50];
                        memset(sub, 0, 15);
                        memset(topic, 0, 50);
                        int sf;
                        sscanf(buf, "%s %s %d", sub, topic, &sf);

                        // Daca am primit o cerere de subscribe
                        if (!strcmp(sub, "subscribe"))
                        {   
                            int subscribed = 0;

                            // Caut clientul care a trimis cererea (dupa fd)
                            for (int k = 0; k < num_tcp_clients; k++)
                            {
                                if (tcp_clients[k].fd == poll_fds[i].fd)
                                { 
                                    // Verific sa nu fie deja abonat la topic
                                    if (check_subscribed(tcp_clients[k].subscriptions, topic) == 0)
                                    {
                                        tcp_clients[k].subscriptions.push_back({strdup(topic), sf});
                                        subscribed = 1;
                                    }
                                    else
                                        fprintf(stderr, "Already subscribed to topic %s\n", topic);

                                    break;
                                }
                            }

                            if (subscribed)
                            {
                                // Trimit mesajul de confirmare catre client
                                memset(buf, 0, MAX_BUF_SIZE);
                                sprintf(buf, "Subscribed to topic.\n");

                                rc = send_all(poll_fds[i].fd, &buf, MAX_BUF_SIZE);
                                DIE(rc < 0, "send");
                            }
                        }
                        // Daca am primit o cerere de unsubscribe
                        else if (!strcmp(sub, "unsubscribe"))
                        {
                            int unsubscribed = 0;

                            // Caut clientul care a trimis cererea (dupa fd)
                            for (int k = 0; k < num_tcp_clients; k++)
                            {
                                if (tcp_clients[k].fd == poll_fds[i].fd)
                                {
                                    vector<pair<char *, int>> subscriptions = tcp_clients[k].subscriptions;
                                    // Caut topicul in lista de abonari a clientului
                                    for (int j = 0; j < subscriptions.size(); j++)
                                        if (!strcmp(subscriptions[j].first, topic))
                                        { // Sterg abonarea la topicul respectiv
                                            tcp_clients[k].subscriptions.erase(tcp_clients[k].subscriptions.begin() + j);
                                            unsubscribed = 1;
                                            break;
                                        }
                                }
                            }

                            if (unsubscribed)
                            {
                                // Trimit mesajul de confirmare catre client
                                memset(buf, 0, MAX_BUF_SIZE);
                                sprintf(buf, "Unsubscribed from topic.\n");

                                rc = send_all(poll_fds[i].fd, &buf, MAX_BUF_SIZE);
                                DIE(rc < 0, "send");
                            }
                            else
                                fprintf(stderr, "Client is not subscribed to topic %s.\n", topic);
                        }
                        else
                            fprintf(stderr, "Unrecognized command.\n");
                    }

                    break;
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    // Dezactivam buffering-ul la afisare
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // Verificam argumentele primite de executabil
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    // Parsam portul primit
    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // Obtinem un socket pentru receptionarea conexiunilor TCP
    int listen_tcp = socket(AF_INET, SOCK_STREAM, 0);
    DIE(listen_tcp < 0, "socket_tcp");

    // Obtinem un socket pentru receptionarea conexiunilor UDP
    int listen_udp = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(listen_udp < 0, "socket_udp");

    // Facem adresa socketilor reutilizabila, ca sa nu primim eroare in caz ca
    // rulam de 2 ori rapid
    // Pentru socket-ul TCP setam si TCP_NODELAY pentru dezactivarea Nagle
    int enable = 1;
    rc = setsockopt(listen_tcp, IPPROTO_TCP, SO_REUSEADDR | TCP_NODELAY, &enable, sizeof(int));
    DIE(rc < 0, "setsockopt(SO_REUSEADDR) tcp failed");

    rc = setsockopt(listen_udp, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    DIE(rc < 0, "setsockopt(SO_REUSEADDR) udp failed");

    // Completăm serv_addr cu adresa serverului pe care am primit-o,
    // familia de adrese (IPv4) si portul pentru conectare
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));

    rc = inet_pton(AF_INET, SERVER_LOCALHOST_IP, &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Asociem adresa serverului cu socketii creati folosind bind
    rc = bind(listen_tcp, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind tcp");

    rc = bind(listen_udp, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind udp");

    // Rulam programul
    run(listen_tcp, listen_udp);

    // Inchidem cei 2 socketi
    close(listen_tcp);
    close(listen_udp);

    return 0;
}