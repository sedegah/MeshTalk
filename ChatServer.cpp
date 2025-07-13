// ChatServer.cpp
#include "common.hpp"

std::map<int, std::string> clients;
std::mutex clients_mutex;

void broadcast(const std::string& message, int sender) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& [fd, name] : clients) {
        if (fd != sender) send(fd, message.c_str(), message.length(), 0);
    }
}

void sendToClient(const std::string& message, int clientSocket) {
    send(clientSocket, message.c_str(), message.length(), 0);
}

void handleClient(int clientSocket) {
    char buffer[BUFFER_SIZE];

    // Ask for username
    sendToClient("Enter username: ", clientSocket);
    memset(buffer, 0, BUFFER_SIZE);
    recv(clientSocket, buffer, BUFFER_SIZE, 0);
    std::string username = buffer;
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

        std::string msg = buffer;
        msg.erase(std::remove(msg.begin(), msg.end(), '\n'), msg.end());

        if (msg == "/quit") break;
        else if (msg == "/who") {
            std::string userList = "Online users:\n";
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (const auto& [fd, name] : clients) userList += " - " + name + "\n";
            sendToClient(userList, clientSocket);
        } else if (msg.rfind("/msg ", 0) == 0) {
            size_t space = msg.find(' ', 5);
            if (space != std::string::npos) {
                std::string target = msg.substr(5, space - 5);
                std::string content = msg.substr(space + 1);

                std::lock_guard<std::mutex> lock(clients_mutex);
                auto it = std::find_if(clients.begin(), clients.end(), [&](auto& p) {
                    return p.second == target;
                });
                if (it != clients.end()) {
                    std::string pm = "[Private] " + username + ": " + content + "\n";
                    send(it->first, pm.c_str(), pm.length(), 0);
                } else {
                    sendToClient("User not found.\n", clientSocket);
                }
            }
        } else {
            std::string finalMsg = username + ": " + msg + "\n";
            broadcast(finalMsg, clientSocket);
        }
    }

    close(clientSocket);
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        std::string leaveMsg = username + " left the chat\n";
        clients.erase(clientSocket);
        broadcast(leaveMsg, -1);
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

    std::cout << "MeshTalk Server started on port " << PORT << std::endl;

    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientSize = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
        std::thread(handleClient, clientSocket).detach();
    }

    close(serverSocket);
    return 0;
}
