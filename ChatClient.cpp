#include "common.hpp"

namespace {
bool performAuthHandshake(int sock, std::string& username, std::string& password, uint64_t& transportKey) {
    std::string serverLine;
    if (!recvLine(sock, serverLine)) {
        std::cerr << "Server disconnected before authentication.\n";
        return false;
    }

    if (serverLine != "AUTH_REQUIRED") {
        std::cerr << "Unexpected server greeting: " << serverLine << "\n";
        return false;
    }

    std::cout << "Username: ";
    std::getline(std::cin, username);
    username = trimLineEndings(username);

    std::cout << "Password (min 6 chars): ";
    std::getline(std::cin, password);
    password = trimLineEndings(password);

    if (username.empty() || password.empty()) {
        std::cerr << "Username/password must not be empty.\n";
        return false;
    }

    if (!sendLine(sock, "AUTH " + username + " " + password)) {
        std::cerr << "Failed to send auth request.\n";
        return false;
    }

    if (!recvLine(sock, serverLine)) {
        std::cerr << "Server disconnected during authentication.\n";
        return false;
    }

    if (serverLine.rfind("AUTH_OK", 0) != 0) {
        std::cerr << "Authentication failed: " << serverLine << "\n";
        return false;
    }

    transportKey = deriveKey(username, password);
    std::cout << "Authenticated successfully (" << serverLine << ").\n";
    return true;
}

void receiveMessages(int socketFd, uint64_t transportKey) {
    std::string line;
    while (recvLine(socketFd, line)) {
        auto plain = decryptFromTransport(line, transportKey);
        if (!plain) {
            std::cout << "\n[warning] Received malformed encrypted frame.\n";
            continue;
        }
        std::cout << *plain << "\n" << std::flush;
    }

    std::cout << "\nDisconnected from server.\n";
    exit(0);
}

void printClientHelp() {
    std::cout
        << "Local client tips:\n"
        << "  /help                -> show server command help\n"
        << "  /who or /online      -> list online users\n"
        << "  /msg <u> <msg>       -> private message\n"
        << "  /history [u|public]  -> view history\n"
        << "  /quit                -> disconnect\n";
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./client <server-ip>\n";
        return 1;
    }

    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket.\n";
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, argv[1], &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid server IP address.\n";
        close(sock);
        return 1;
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed.\n";
        close(sock);
        return 1;
    }

    std::string username;
    std::string password;
    uint64_t transportKey = 0;
    if (!performAuthHandshake(sock, username, password, transportKey)) {
        close(sock);
        return 1;
    }

    printClientHelp();
    std::thread(receiveMessages, sock, transportKey).detach();

    std::string input;
    while (true) {
        std::getline(std::cin, input);
        input = trimLineEndings(input);
        if (input.empty()) continue;

        const std::string payload = encryptForTransport(input, transportKey);
        if (!sendLine(sock, payload)) {
            std::cerr << "Failed to send message.\n";
            break;
        }

        if (input == "/quit") break;
    }

    close(sock);
    return 0;
}
