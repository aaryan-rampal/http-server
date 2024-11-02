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

enum 
{
    STATE_REQ = 0, // reading request
    STATE_RES = 1, // sending response
    STATE_END = 2
};

struct Conn 
{
    int fd = -1;
    uint32_t state = 0; // STATE_RES or STATE_REQ
    // reading buffer
    size_t rbuf_size = 0;
    uint8_t rbuf[4 + k_max_msg];
    // writing buffer
    size_t wbuf_size = 0;
    size_t wbuf_sent = 0;
    uint8_t wbuf[4 + k_max_msg];
};


const size_t k_max_msg = 4096;

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

static int32_t one_request(int connfd)
{
    /*
    +-----+------+-----+------+--------
    | len | msg1 | len | msg2 | more...
    +-----+------+-----+------+--------
    This is how our request will look like
    */

    // the header will be 4 bytes to determine size of msg
    // last byte for eof indicator
    char rbuf[4 + k_max_msg + 1];
    errno = 0;
    int32_t err = read_full(connfd, rbuf, 4);
    if (err)
    {
        if (errno == 0)
        {
            printf("EOF");
        }
        else
        {
            printf("read() error");
        }
        return err;
    }

    // ensure size of msg < k_max_msg
    uint32_t len = 0;
    memcpy(&len, rbuf, 4);
    if (len > k_max_msg)
    {
        printf("msg too long");
        return -1;
    }

    // reads the len long msg starting from rbuf[4]
    err = read_full(connfd, &rbuf[4], len);
    if (err)
    {
        printf("read error");
        return err;
    }

    // add eof indicator after the message length
    rbuf[4 + len] = '\0';
    printf("client says: %s\n", &rbuf[4]);

    const char reply[] = "world";
    char wbuf[4 + sizeof(reply)];
    len = (uint32_t)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);
    return write_all(connfd, wbuf, 4 + len);
}

static void die(const char *s)
{
    printf("Error: %s\n", strerror(errno));
    printf("%s\n", s);
    exit(1);
}

static void do_something(int connfd)
{
    char rbuf[64] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0)
    {
        printf("read() error");
        return;
    }
    printf("client says: %s\n", rbuf);

    char wbuf[] = "world";
    write(connfd, wbuf, strlen(wbuf));
}

int main()
{
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
    if (rv)
    {
        die("bind()");
    }

    /* Listenting on our socket */
    // second param is size of queue of backlog requests that our socket has received
    // SOMAXCONN is 128 on unix
    rv = listen(fd, SOMAXCONN);
    if (rv)
    {
        die("listen()");
    }

    while (true)
    {
        // accept connection
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        // same thing as before
        struct sockaddr *ptr_clt_addr = (struct sockaddr *)&client_addr;
        int connfd = accept(fd, ptr_clt_addr, &addrlen);
        if (connfd < 0)
        {
            // error
            printf("connfd is less than 0");
            continue;
        }

        while (true)
        {
            int32_t err = one_request(connfd);
            if (err)
            {
                break;
            }
        }
        close(connfd);
    }

    return 0;
}