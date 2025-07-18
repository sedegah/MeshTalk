#include "common.hpp"

void receiveMessages(int socket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes = recv(socket, buffer, BUFFER_SIZE, 0);
        if (bytes > 0) {
            std::cout << buffer << std::flush;
        } else {
            std::cout << "\nDisconnected from server.\n";
            exit(0);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./client <server-ip>\n";
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, argv[1], &serverAddr.sin_addr);

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed.\n";
        return 1;
    }

    std::thread(receiveMessages, sock).detach();

    std::string input;
    while (true) {
        std::getline(std::cin, input);
        if (input.empty()) continue;
        send(sock, input.c_str(), input.size(), 0);
        if (input == "/quit") break;
    }

    close(sock);
    return 0;
}
