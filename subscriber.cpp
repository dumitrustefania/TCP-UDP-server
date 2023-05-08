#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"

void run_client(int sockfd)
{

    struct pollfd poll_fds[2];
    int num_clients = 2;

    poll_fds[0].fd = 0;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = sockfd;
    poll_fds[1].events = POLLIN;

    while (1)
    {
        int rc = poll(poll_fds, num_clients, -1);
        DIE(rc < 0, "poll");

        for (int i = 0; i < num_clients; i++)
        {
            if (poll_fds[i].revents && POLLIN)
            {
                if (poll_fds[i].fd == 0)
                {
                    char buf[1800];
                    memset(buf, 0, 1800);
                    fgets(buf, sizeof(buf), stdin);

                    if (!strncmp(buf, "exit", 4))
                        return;
                    
                    // subscribe/unsubscribe
                    // Use send_all function to send the pachet to the server.
                    send_all(sockfd, &buf, 1800);
                }
                else
                {
                    char buf[1800];
                    memset(buf, 0, 1800);
                    int rc = recv_all(sockfd, &buf, 1800);
                    if (rc <= 0)
                        break;

                    if (!strncmp(buf, "exit", 4)) 
                       return;
                    

                    printf("%s", buf);
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sockfd = -1;

    if (argc != 4)
    {
        printf("\n Usage: %s <id> <ip> <port>\n", argv[0]);
        return 1;
    }

    // Parsam port-ul ca un numar
    uint16_t port;
    int rc = sscanf(argv[3], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // Obtinem un socket TCP pentru conectarea la server
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    // Completăm in serv_addr adresa serverului, familia de adrese si portul
    // pentru conectare
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    // Ne conectăm la server
    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connect");

    // trimit id ul la server
    send_all(sockfd, argv[1], sizeof(argv[1]));

    run_client(sockfd);

    // Inchidem conexiunea si socketul creat
    close(sockfd);

    return 0;
}
