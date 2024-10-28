#ifndef SimpleSocket_hpp
#define SimpleSocket_hpp

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>

namespace HDE {
    class SimpleSocket {
        public:
            SimpleSocket(int domain, int service, int protocol, int port, u_long interface);
            virtual int establish_network_connection(int sock, struct sockaddr_in address) = 0;
            void test_connection(int to_test);
            struct sockaddr_in get_address();
            int get_sock();
            int get_connection();
        private:
            struct sockaddr_in address;
            int sock;
            int connection;
    };
}

#endif