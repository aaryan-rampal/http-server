#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

static void die(const char* s) {
    printf("Error: %s\n", strerror(errno));
    printf("%s\n", s);
    exit(1);
}

static void do_something(int connfd) {
    char rbuf[64] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        printf("read() error");
        return;
    }
    printf("client says: %s\n", rbuf);

    char wbuf[] = "world";
    write(connfd, wbuf, strlen(wbuf));
}

int main() {
    /* Creating our socket */

    // first param dictates IPv4/IPv6, second one is UDP/TCP, third is {protocol}?
    int domain = AF_INET;
    int fd = socket(domain, SOCK_STREAM, 0);

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    uint16_t port = 1234;
    uint32_t address = 0;
    struct sockaddr_in addr = {};

    addr.sin_family = domain;
    // binds to port 1234
    addr.sin_port = ntohs(port);
    // sets address to 0.0.0.0
    addr.sin_addr.s_addr = ntohl(address);

    // find memory location of addr (&addr) and cast it as a pointer of type sockaddr_in
    const sockaddr *ptr_addr = (const sockaddr *)&addr;
    int rv = bind(fd, ptr_addr, sizeof(addr));
    if (rv) {
        die("bind()");
    }


    /* Listenting on our socket */
    // second param is size of queue of backlog requests that our socket has received
    // SOMAXCONN is 128 on unix
    rv = listen(fd, SOMAXCONN);
    if(rv) {
        die("listen()");
    }


    while (true) {
        // accept connection
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        // same thing as before
        struct sockaddr *ptr_clt_addr = (struct sockaddr *)&client_addr;
        int connfd = accept(fd, ptr_clt_addr, &addrlen);
        if (connfd < 0) {
            // error
            printf("connfd is less than 0");
            continue;
        }
        do_something(connfd);
        close(connfd);
    }

    return 0;





}