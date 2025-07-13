#include "common.hpp"

std::map<int, std::string> clients;
std::mutex clients_mutex;

void broadcast(const std::string& message, int senderFd = -1) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& [fd, name] : clients) {
        if (fd != senderFd) send(fd, message.c_str(), message.length(), 0);
    }
}

void sendToClient(int clientSocket, const std::string& message) {
    send(clientSocket, message.c_str(), message.length(), 0);
}

void handleClient(int clientSocket) {
    char buffer[BUFFER_SIZE];
    sendToClient(clientSocket, "Enter username: ");
    recv(clientSocket, buffer, BUFFER_SIZE, 0);
    std::string username = std::string(buffer);
    username.erase(std::remove(username.begin(), username.end(), '\n'), username.end());

    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients[clientSocket] = username;
    }

    std::string joinMsg = username + " joined the chat\n";
    broadcast(joinMsg, clientSocket);

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytes <= 0) break;

        std::string msg(buffer);
        msg.erase(std::remove(msg.begin(), msg.end(), '\n'), msg.end());

        if (msg == "/quit") break;
        else if (msg == "/who") {
            std::string userList = "Online users:\n";
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (const auto& [fd, name] : clients) {
                userList += " - " + name + "\n";
            }
            sendToClient(clientSocket, userList);
        } else if (msg.rfind("/msg ", 0) == 0) {
            size_t split = msg.find(' ', 5);
            if (split != std::string::npos) {
                std::string target = msg.substr(5, split - 5);
                std::string content = msg.substr(split + 1);

                std::lock_guard<std::mutex> lock(clients_mutex);
                auto it = std::find_if(clients.begin(), clients.end(),
                    [&](const auto& pair) { return pair.second == target; });

                if (it != clients.end()) {
                    std::string privMsg = "[Private] " + username + ": " + content + "\n";
                    send(it->first, privMsg.c_str(), privMsg.length(), 0);
                } else {
                    sendToClient(clientSocket, "User not found.\n");
                }
            }
        } else {
            std::string broadcastMsg = username + ": " + msg + "\n";
            broadcast(broadcastMsg, clientSocket);
        }
    }

    close(clientSocket);
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        std::string leaveMsg = username + " left the chat\n";
        clients.erase(clientSocket);
        broadcast(leaveMsg);
    }
}
    
int main() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 5);

    std::cout << "MeshTalk Server running on port " << PORT << "...\n";

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientSize = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
        std::thread(handleClient, clientSocket).detach();
    }

    close(serverSocket);
    return 0;
}
