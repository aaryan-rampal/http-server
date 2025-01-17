// stdlib
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
// system
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
// C++
#include <vector>


struct Conn {
    int fd = -1;

    // application's intention, for the event loop
    bool want_read = false;
    bool want_write = false;
    // tells event loop to destroy connection
    bool want_close = false;

    // buffered io
    std::vector<uint8_t> incoming;  // input from read
    std::vector<uint8_t> outgoing;  // output to write
};

void msg(const char *message)
{
    fprintf(stderr, "%s\n", message);
}

void die(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

const size_t k_max_msg = 4096;

// static int32_t read_full(int fd, char *buf, size_t n)
// {
//     while (n > 0)
//     {
//         // read returns the number of bytes read
//         ssize_t rv = read(fd, buf, n);
//         if (rv <= 0)
//         {
//             // error -> rv == -1
//             // EOF -> rv == 0
//             return -1;
//         }
//         assert((size_t)rv <= n);
//         n -= (size_t)rv;
//         buf += rv;
//     }
//     return 0;
// }

// static int32_t write_all(int fd, const char *buf, size_t n)
// {
//     while (n > 0)
//     {
//         ssize_t rv = write(fd, buf, n);
//         if (rv <= 0)
//         {
//             return -1; // error
//         }
//         assert((size_t)rv <= n);
//         n -= (size_t)rv;
//         buf += rv;
//     }
//     return 0;
// }

// static int32_t one_request(int connfd)
// {
//     // 4 bytes for header, 1 byte for null terminator
//     char rbuf[4 + k_max_msg + 1];
//     errno = 0;
//     int32_t err = read_full(connfd, rbuf, 4);
//     if (err)
//     {
//         if (errno == 0)
//         {
//             msg("EOF");
//         }
//         else
//         {
//             msg("read() error");
//         }
//         return err;
//     }

//     uint32_t len = 0;
//     memcpy(&len, rbuf, 4); // assume little endian
//     if (len > k_max_msg)
//     {
//         msg("too long");
//         return -1;
//     }

//     // request body
//     err = read_full(connfd, &rbuf[4], len);
//     if (err)
//     {
//         msg("read() error");
//         return err;
//     }

//     // do something
//     rbuf[4 + len] = '\0';
//     printf("client says: %s\n", &rbuf[4]);

//     // reply using the same protocol
//     const char reply[] = "world";
//     char wbuf[4 + sizeof(reply)];
//     len = (uint32_t)strlen(reply);
//     // wbuf is the same as &wbuf[0]
//     memcpy(wbuf, &len, 4);
//     memcpy(&wbuf[4], reply, len);
//     return write_all(connfd, wbuf, 4 + len);
// }

static void do_something(int connfd)
{
    // connfd is a file descriptor for the connected socket
    char rbuf[64] = {};
    // leaves 1 byte for null terminator
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0)
    {
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

    // map of the client connected, keyed by fd
    std::vector<Conn *> fd2conn;
    std::vector<struct pollfd> poll_args;
    while (true) {
        // remove any existing values
        poll_args.clear();

        // this is the listening sockets, which I want first
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);

        // now we have connection sockets
        for (Conn *conn : fd2conn) {
            // if conn is NULL, move on
            if (!conn) {
                continue;
            }

            // POLLERR here in case conn does not want_read or want_write
            // because then it shouldn't be here
            struct pollfd pfd = {conn->fd, POLLERR, 0};

            // ideally only one of these is true, and we set it's value
            if (conn->want_read) {
                pfd.events |= POLLIN;
            }
            if (conn->want_write) {
                pfd.events |= POLLOUT;
            }
            poll_args.push_back(pfd);
        }


        // this block waits for the readiness of the fds
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
        if (rv < 0 && errno == EINTR) {
            // not an error, no fds are ready
            continue;
        }
        if (rv < 0) {
            die("poll");
        }

        if (poll_args[0].revents) {
            if (Conn *conn = handle_accept(fd)) {
                // resize vector to make sure it can handle the new conn->fd
                if (fd2conn.size() <= (size_t)conn->fd) {
                    fd2conn.resize(conn->fd + 1);
                }

                // put it into the map keyed by fd
                fd2conn[conn->fd] = conn;
            }
        }

    }
};