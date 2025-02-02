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
#include <string>
#include <map>

const size_t k_max_msg = 32 << 20;  // likely larger than the kernel buffer
const size_t k_max_args = 200 * 1000;

// Response::status
enum {
    RES_OK = 0,
    RES_ERR = 1,    // error
    RES_NX = 2,     // key not found
};

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

struct Response {
    uint32_t status = 0;
    std::vector<uint8_t> data;
};

static std::map<std::string, std::string> g_data;

static void do_request(std::vector<std::string> &cmd, Response &out) {
    if (cmd.size() == 2 && cmd[0] == "get") {
        // GET key request for redis
        auto it = g_data.find(cmd[1]);

        // did not find key in map
        if (it == g_data.end()) {
            out.status = RES_NX;
            return;
        }

        // assign the value to the response
        const std::string &val = it->second;
        out.data.assign(val.begin(), val.end());
    } else if (cmd.size() == 3 && cmd[0] == "set") {
        // SET key value request for redis
        g_data[cmd[1]].swap(cmd[2]);
        const std::string &val = g_data.find(cmd[1])->second;
        out.data.assign(val.begin(), val.end());
        out.status = RES_OK;
    } else if (cmd.size() == 2 && cmd[0] == "del") {
        // DEL key request for redis
        g_data.erase(cmd[1]);
        out.status = RES_OK;
    } else {
        // unrecognized command
        out.status = RES_ERR;
    }
}

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

static bool read_u32(const uint8_t *&curr, const uint8_t *end, uint32_t &out) {
    // if there isn't enough space to read 4 bytes, return false
    if (curr + 4 > end) {
        return false;
    }

    // update out and curr
    memcpy(&out, curr, 4);
    curr += 4;
    return true;
}

static bool read_str(const uint8_t *&curr, const uint8_t *end, size_t n, std::string &out) {
    if (curr + n > end) {
        return false;
    }
    out.assign(curr, curr + n);
    curr += n;
    return true;
}

static int32_t parse_req(const uint8_t *data, size_t size, std::vector<std::string> &out) {
    // find end pointer of data
    const uint8_t *end = data + size;

    // find num of arguments in the request
    uint32_t nstr = 0;
    // if you can't read the number of arguments, return -1
    if (!read_u32(data, end, nstr)) {
        return -1;
    }
    // if the number of arguments is greater than the max, return -1
    if (nstr > k_max_args) {
        return -1;
    }

    // read the arguments
    while (out.size() < nstr) {
        uint32_t len = 0;  
        if (!read_u32(data, end, len)) {
            return -1;
        }
        out.push_back(std::string());
        if(!read_str(data, end, len, out.back())) {
            return -1;
        }
    }

    // if there's still data left, return -1
    if (data != end) {
        return -1;
    }
    return 0;

}

static void make_response(const Response &resp, std::vector<uint8_t> &out) {
    uint32_t resp_len = 4 + (uint32_t)resp.data.size();

    printf("Response Length: %u\n", resp_len);
    printf("Response Status: %u\n", resp.status);
    std::string response_data(resp.data.begin(), resp.data.end());
    printf("Response Data (%lu bytes): %s\n", resp.data.size(), response_data.c_str());
    // appends response length, then response status and finally response data to buffer
    buf_append(out, (const uint8_t *)&resp_len, 4);
    buf_append(out, (const uint8_t *)&resp.status, 4);
    buf_append(out, resp.data.data(), resp.data.size());
}

static bool try_one_request(Conn *conn) {
    // try to parse the protocol: message header
    if (conn->incoming.size() < 4) {
        return false;   // want read
    }
    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);
    if (len > k_max_msg) {
        msg("too long");
        conn->want_close = true;
        return false;   // want close
    }
    // message body
    if (4 + len > conn->incoming.size()) {
        return false;   // want read
    }
    const uint8_t *request = &conn->incoming[4];

    // got one request, do some application logic
    std::vector<std::string> cmd;
    if (parse_req(request, len, cmd) < 0) {
        conn->want_close = true;
        return false;
    }

    Response resp;
    do_request(cmd, resp);
    make_response(resp, conn->outgoing);

    buf_consume(conn->incoming, 4 + len);
    // everything went well
    return true;
}

static void handle_write(Conn *conn) {
    // make sure we have something to write
    assert(conn->outgoing.size() > 0);

    ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());

    if (rv < 0 && errno == EAGAIN) {
        // not actually ready
        return;
    }

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

static void handle_read(Conn *conn) {
    // want to do a non-blocking read
    uint8_t buf[64 * 1024];
    ssize_t rv = read(conn->fd, buf, sizeof(buf));

    if (rv < 0 && errno == EAGAIN) {
        return; // actually not ready
    }
    // handle IO error
    if (rv < 0) {
        msg_errno("read() error");
        conn->want_close = true;
        return; // want close
    }
    // handle EOF
    if (rv == 0) {
        if (conn->incoming.size() == 0) {
            msg("client closed");
        } else {
            msg("unexpected EOF");
        }
        conn->want_close = true;
        return; // want close
    }

    // add new data to the incoming buffer for connection
    buf_append(conn->incoming, buf, (size_t)rv);

    // instead of assuming we only have one request, we will
    // implement pipelining by treating input as byte stream
    while (try_one_request(conn)) {
    }

    if (conn->outgoing.size() > 0) {
        conn->want_read = false;
        conn->want_write = true;

        // socket likely ready to write so do it
        return handle_write(conn);
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