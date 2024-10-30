#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

void die(const char* s) {
    printf("calling die for %s\n", s);
}

int main() {
    int domain = AF_INET;
    int fd = socket(domain, SOCK_STREAM, 0);

    int port = 1234;
    struct sockaddr_in addr = {};
    addr.sin_family = domain;
    // binds to port 1234
    addr.sin_port = ntohs(port);
    // sets address to 127.0.0.1
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);

    // find memory location of addr (&addr) and cast it as a pointer of type sockaddr_in
    const sockaddr *ptr_addr = (const sockaddr *)&addr;
    int rv = bind(fd, ptr_addr, sizeof(addr));
    if (rv) {
        perror("bind() error");
        die("bind()");
    }

    char msg[] = "hello";
    write(fd, msg, strlen(msg));

    char rbuf[64] = {};
    ssize_t n = read(fd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        die("read");
    }
    printf("server says: %s\n", rbuf);
    close(fd);
}