#include "SimpleSocket.hpp"

HDE::SimpleSocket::SimpleSocket(int domain, int service, int protocol, int port, u_long interface) {
    // define address
    address.sin_family = domain;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(interface);

    // establish and test socket
    sock = socket(domain, service, protocol);
    test_connection(sock);

    // establish and test network connection
    connection = establish_network_connection(sock, address);
    test_connection(connection);
}

void HDE::SimpleSocket::test_connection(int to_test) {
    if (to_test < 0) {
        perror("Failed to connect");
        exit(EXIT_FAILURE);
    }
}

// getter functions

int HDE::SimpleSocket::get_sock() {
    return sock;
}

int HDE::SimpleSocket::get_connection() {
    return connection;
}

struct sockaddr_in HDE::SimpleSocket::get_address() {
    return address;
}