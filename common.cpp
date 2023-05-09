#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

/*
   Foloseste functia recv pana se asigura ca au fost cititi
   len octeti sau ca nu mai are ce citi in buffer. 
*/
int recv_all(int sockfd, void *buffer, size_t len)
{
    size_t bytes_received = 0;
    size_t bytes_remaining = len;
    void *buff = buffer;

    while (bytes_remaining)
    {
        int bytes = recv(sockfd, buff, bytes_remaining, 0);

        // eroare
        if (bytes < 0)
            return bytes;

        // nu mai avem ce sa scriem in buffer de pe socket
        if (bytes == 0)
            break;

        bytes_received += bytes;
        bytes_remaining -= bytes;

        // Cast void* la char* deoarece
        // nu se pot face adunari la void* in cpp
        char *charPtr = static_cast<char *>(buff);
        charPtr += bytes;
        // Cast char* inapoi la void*
        buff = static_cast<void *>(charPtr);
    }

    return bytes_received;
}

/*
   Foloseste functia send pana se asigura ca au fost trimisi
   len octeti sau ca nu mai are ce trimite din buffer. 
*/
int send_all(int sockfd, void *buffer, size_t len)
{
    size_t bytes_sent = 0;
    size_t bytes_remaining = len;
    void *buff = buffer;

    while (bytes_remaining)
    {
        int bytes = send(sockfd, buff, bytes_remaining, 0);

        // eroare
        if (bytes < 0)
            return bytes;

        // nu mai avem ce sa citim din buffer
        if (bytes == 0)
            break;

        bytes_sent += bytes;
        bytes_remaining -= bytes;
        
        // Cast void* la char* deoarece
        // nu se pot face adunari la void* in cpp
        char *charPtr = static_cast<char *>(buff);
        charPtr += bytes;
        // Cast char* inapoi la void*
        buff = static_cast<void *>(charPtr);
    }

    return bytes_sent;
}