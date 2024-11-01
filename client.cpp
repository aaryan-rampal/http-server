#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <assert.h>

const size_t k_max_msg = 4096;

void die(const char *s)
{
    printf("Error: %s\n", strerror(errno));
    printf("%s\n", s);
    exit(1);
}

static int32_t read_full(int fd, char *buf, size_t n)
{
    while (n > 0)
    {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0)
        {
            return -1;
        }

        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

// TODO: understand this one
static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}


static int32_t query(int fd, const char *text)
{
    // make sure our query is under the max message
    uint32_t len = (uint32_t)strlen(text);
    if (len > k_max_msg)
    {
        return -1;
    }

    char wbuf[4 + k_max_msg];
    // copy length into buffer
    memcpy(wbuf, &len, 4);
    // copy text into buffer
    memcpy(&wbuf[4], text, len);
    if (int32_t err = write_all(fd, wbuf, 4 + len))
    {
        return err;
    }

    char rbuf[4 + k_max_msg + 1];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if (err) {
        if (errno == 0) {
            printf("EOF");
        } else {
            printf("read() error");
        }
        return err;
    }

    memcpy(&len, rbuf, 4);
    if (len > k_max_msg) {
        printf("response too long");
        return -1;
    }

    err = read_full(fd, &rbuf[4], len);
    if (err) {
        printf("read() error");
        return err;
    }

    rbuf[4+len] = '\0';
    printf("server says %s\n", &rbuf[4]);
    return 0;
}

int main()
{
    int domain = AF_INET;
    int fd = socket(domain, SOCK_STREAM, 0);
    if (fd < 0)
    {
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
    if (rv)
    {
        die("connect()");
    }

    // char msg[] = "hello";
    // write(fd, msg, strlen(msg));

    // char rbuf[64] = {};
    // ssize_t n = read(fd, rbuf, sizeof(rbuf) - 1);
    // if (n < 0)
    // {
    //     die("read");
    // }
    // printf("server says: %s\n", rbuf);
    // close(fd);
    int32_t err = query(fd, "hello1");
    if (err) {
        goto L_DONE;
    }
    err = query(fd, "hello2");
    if (err) {
        goto L_DONE;
    }
    err = query(fd, "hello3");
    if (err) {
        goto L_DONE;
    }

L_DONE:
    close(fd);
    return 0;
}