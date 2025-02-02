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
#include <vector>
#include <string>

// const size_t k_max_msg = 4096;
const size_t k_max_msg = 32 << 20;  // likely larger than the kernel buffer

void msg(const char *message)
{
    fprintf(stderr, "%s\n", message);
}

void die(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

static int32_t read_full(int fd, uint8_t *buf, size_t n)
{
    while (n > 0)
    {
        // read returns the number of bytes read
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0)
        {
            // error -> rv == -1
            // EOF -> rv == 0
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    // inserts data to data + len into the end of the buffer
    buf.insert(buf.end(), data, data + len);
}

static void buf_consume(std::vector<uint8_t> &buf, size_t len) {
    // removes length len from the beginning of the buffer
    buf.erase(buf.begin(), buf.begin() + len);
}

static int32_t send_req(int fd, const uint8_t *text, size_t len)
{
    // reads the length and ensures it's under our max limit
    uint32_t nlen = (uint32_t)len;
    if (nlen > k_max_msg)
    {
        return -1;
    }

    // write length and text to the buffer
    std::vector<uint8_t> wbuf;
    buf_append(wbuf, (const uint8_t *)&nlen, 4);
    buf_append(wbuf, text, nlen);
    return 0;
}

static int32_t read_res(int fd) {
    std::vector<uint8_t> rbuf;
    rbuf.resize(4);
    errno = 0;
    // checks whether we have proper TCP protocols
    int32_t err = read_full(fd, &rbuf[0], 4);
    if (err) {
        if (errno == 0) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return err;
    }

    // read the length of the message
    uint32_t len = 0;
    memcpy(&len, &rbuf[0], 4);
    if (len > k_max_msg) {
        msg("too long");
        return -1;
    }

    // read the text in the buffer
    rbuf.resize(4 + len);
    err = read_full(fd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }

    // do something
    printf("len:%u data:%.*s\n", len, len < 100 ? len : 100, &rbuf[4]);
    return 0;
}

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK); // 127.0.0.1
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv)
    {
        die("connect");
    }

    // multiple pipelined requests
    // std::vector<std::string> query_list = {
    //     "hello",
    //     "world",
    //     "foo",
    //     "bar",
    //     "baz",
    // };
    std::vector<std::string> query_list = {
        "hello1", "hello2", "hello3",
        std::string(k_max_msg, 'z'), // requires multiple event loop iterations
        "hello5",
    };
    for (const std::string &s : query_list) {
        int32_t err = send_req(fd, (const uint8_t *)s.data(), s.size());
        if (err)
        {
            goto L_DONE;
        }
    }
    for (size_t i = 0; i < query_list.size(); i++) {
        int32_t err = read_res(fd);
        if (err)
        {
            goto L_DONE;
        }
    }


    return 0;

L_DONE:
    close(fd);
    return 0;
}
