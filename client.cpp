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
    printf("Error: %s\n", strerror(errno));
    printf("%s\n", s);
    exit(1);
}

int main() {
    int domain = AF_INET;
    int fd = socket(domain, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    uint16_t port = 1234;
    uint32_t address = INADDR_LOOPBACK;
    struct sockaddr_in addr = {};

    addr.sin_family = domain;
    addr.sin_port = ntohs(port);
    addr.sin_addr.s_addr = ntohl(address);

    // find memory location of addr (&addr) and cast it as a pointer of type sockaddr_in
    const struct sockaddr *ptr_addr = (const struct sockaddr *)&addr;
    int rv = connect(fd, ptr_addr, sizeof(addr));
    if (rv) {
        die("connect()");
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