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

const size_t k_max_msg = 4096;

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

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void msg_errno(const char *msg) {
    fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

static void die(const char *msg) {
    fprintf(stderr, "[%d] %s\n", errno, msg);
    abort();
}

static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    // inserts data to data + len into the end of the buffer
    buf.insert(buf.end(), data, data + len);
}

static void buf_consume(std::vector<uint8_t> &buf, size_t len) {
    // removes length len from the beginning of the buffer
    buf.erase(buf.begin(), buf.begin() + len);
}

static bool try_one_request(Conn *conn) {
    // try to parse the buffer
    if (conn->incoming.size() < 4) {
        return false;
    }

    // get length of the incoming data and ensure it's under our max limit
    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);
    if (len > k_max_msg) {
        msg("too long");
        conn->want_close = true;
        return false;
    }

    // our protocol dictates we must have a 4 byte header + len bytes of data
    if (4 + len > conn->incoming.size()) {
        return false;
    }

    const uint8_t *request = &conn->incoming[4];
    // got one request, do some application logic
    printf("client says: len:%d data:%.*s\n",
        len, len < 100 ? len : 100, request);

    // generate response and add it to the outgoing buffer
    // currently response just echoes back to the client what they said
    buf_append(conn->outgoing, (const uint8_t *)&len, 4);
    buf_append(conn->outgoing, request, len);

    // remove the request from the incoming buffer
    buf_consume(conn->incoming, 4 + len);

    // everything went well
    return true;
}

static void handle_read(Conn *conn) {
    // want to do a non-blocking read
    uint8_t buf[64 * 1024];
    ssize_t rv = read(conn->fd, buf, sizeof(buf));

    if (rv <= 0) {
        // if there's an error, we just close the connection?
        conn->want_close = true;
        return;
    }

    // add new data to the incoming buffer for connection
    buf_append(conn->incoming, buf, (size_t)rv);
    try_one_request(conn);

    // still has data to write, won't read until data has been written
    if (conn->outgoing.size() > 0) {
        conn->want_read = false;
        conn->want_write = true;
    }
}

static void handle_write(Conn *conn) {
    // make sure we have something to write
    assert(conn->outgoing.size() > 0);

    ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());
    // if we can't write, we just close the connection
    if (rv < 0) {
        conn->want_close = true;
        return;
    }

    // remove the data which we have written from the outgoing buffer
    buf_consume(conn->outgoing, (size_t)rv);    

    // has written all data, wants to go back to reading
    if (conn->outgoing.size() == 0) {
        conn->want_read = true;
        conn->want_write = false;
    }
}

// make listening socket non blocking
static void fd_set_nb(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}


static Conn *handle_accept(int fd) {
    // boilerplate code for handling accept
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
    if (connfd < 0) {
        return NULL;
    }

    uint32_t ip = client_addr.sin_addr.s_addr;
    fprintf(stderr, "new client from %u.%u.%u.%u:%u\n",
        ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24,
        ntohs(client_addr.sin_port)
    );

    // set this new connection fd to non blocking mode
    fd_set_nb(connfd);

    // create custom Conn struct and return it
    Conn *conn = new Conn();
    conn->fd = connfd;
    conn->want_read = true;
    return conn;
}


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
                // resize vector to make sure it can handle the listening socket (fd)
                if (fd2conn.size() <= (size_t)conn->fd) {
                    fd2conn.resize(conn->fd + 1);
                }

                // put it into the vec keyed by fd
                fd2conn[conn->fd] = conn;
            }
        }

        // skip the 1st (fd), which we set up manually
        for (size_t i = 1; i < poll_args.size(); i++) {
            // current connection pollfd struct
            struct pollfd curr = poll_args[i];
            uint32_t ready = curr.revents;
            Conn *conn = fd2conn[curr.fd];

            // if conn is ready, then read/write to it based on flags
            if (ready & POLLIN) {
                handle_read(conn);
            }
            if (ready & POLLOUT) {
                handle_write(conn);
            }

            // delete conn from fd2conn if we have POLLERR or the connection wants to close
            if (ready & POLLERR || conn->want_close) {
                (void)close(conn->fd);
                fd2conn[conn->fd] = NULL;
                delete conn;
            }
        }

    }
};