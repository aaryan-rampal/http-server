#include <iostream>
#include <vector>
#include <thread>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>

const int CLIENT_COUNT = 500;  // Adjust this to test server limits
const int REQUESTS_PER_CLIENT = 10;  // How many requests each client sends

void client_task(int id) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "Client " << id << " failed to create socket\n";
        return;
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Client " << id << " failed to connect\n";
        close(fd);
        return;
    }

    for (int i = 0; i < REQUESTS_PER_CLIENT; i++) {
        std::string message = "Hello from client " + std::to_string(id);
        uint32_t len = message.length();

        // Send message length first
        write(fd, &len, 4);
        // Send message
        write(fd, message.c_str(), len);

        // Read server response
        char buffer[1024] = {0};
        read(fd, buffer, sizeof(buffer));
        std::cout << "Client " << id << " received: " << buffer << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Small delay
    }

    close(fd);
}

int main() {
    std::vector<std::thread> clients;

    for (int i = 0; i < CLIENT_COUNT; i++) {
        clients.emplace_back(client_task, i);
    }

    for (auto &client : clients) {
        client.join();
    }

    std::cout << "Test completed\n";
    return 0;
}
