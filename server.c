#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void msg(const char *message) {
    fprintf(stderr, "%s\n", message);
}

void die(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

static void do_something(int connfd) {
    char rbuf[64] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        msg("read() error");
        return;
    }
    printf("client says: %s\n", rbuf);

    char wbuf[] = "world";
    write(connfd, wbuf, strlen(wbuf));
}


int main()
{
    // AF_INET: IPv4
    // SOCK_STREAM: TCP
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        die("socket()");
    }

    // Set the SO_REUSEADDR option to allow binding to an address that is already in use
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0); // wildcard address 0.0.0.0

    // changed (const sockaddr *) to (const struct sockaddr *)
    // bind() associates the socket with the address and port number specified in addr
    int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv == -1)
    {
        die("bind()");
    }

    // actually listening to clients now
    // SOMAXCONN (128) is the maximum number of pending connections that can be queued up before connections are refused
    rv = listen(fd, SOMAXCONN);
    if (rv == -1)
    {
        die("listen()");
    }

    while (1)
    {
        // accept
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        // accept() blocks until a client connects to the server
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
        if (connfd == -1)
        {
            continue; // error
        }

        do_something(connfd);
        close(connfd);
    }
};