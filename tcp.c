#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

#include "tcp.h"

int lookup(char *host, char *portnr, struct addrinfo **res)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;
    hints.ai_protocol = 0;

    return getaddrinfo(host, portnr, &hints, res);
}

int connect_to(struct addrinfo *addr, struct timeval *rtt, int timeout) {
    int sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    // Set the socket to non-blocking mode
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    struct timeval start, now;
    gettimeofday(&start, NULL);

    int result = connect(sockfd, addr->ai_addr, addr->ai_addrlen);
    if (result < 0 && errno != EINPROGRESS) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(sockfd, &write_fds);

    struct timeval timeout_tv;
    timeout_tv.tv_sec = timeout;
    timeout_tv.tv_usec = 0;

    int select_result = select(sockfd + 1, NULL, &write_fds, NULL, &timeout_tv);
    if (select_result <= 0) {
        // Connection timed out or error occurred
        close(sockfd);
        return -1;
    }

    if (FD_ISSET(sockfd, &write_fds)) {
        // Connection succeeded
        gettimeofday(&now, NULL);
        timersub(&now, &start, rtt);
        return 0;
    } else {
        // Connection failed
        close(sockfd);
        return -1;
    }
}

