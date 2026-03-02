#include "common.hpp"

namespace {
struct ClientSession {
    std::string username;
    uint64_t transportKey = 0;
};

std::map<int, ClientSession> clients;
std::mutex clientsMutex;

std::deque<std::string> publicHistory;
std::map<std::string, std::deque<std::string>> privateHistory;
std::mutex historyMutex;

std::unordered_map<std::string, std::string> userHashes;
std::mutex authMutex;

std::string privateHistoryKey(const std::string& a, const std::string& b) {
    return (a < b) ? (a + "|" + b) : (b + "|" + a);
}

void pushHistory(std::deque<std::string>& history, const std::string& entry) {
    history.push_back(entry);
    if (history.size() > HISTORY_LIMIT) history.pop_front();
}

bool sendEncrypted(int clientSocket, const std::string& plaintext) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    const auto it = clients.find(clientSocket);
    if (it == clients.end()) return false;
    return sendLine(clientSocket, encryptForTransport(plaintext, it->second.transportKey));
}

void broadcastEncrypted(const std::string& plaintext, int senderFd = -1) {
    std::vector<std::pair<int, uint64_t>> recipients;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        recipients.reserve(clients.size());
        for (const auto& [fd, session] : clients) {
            if (fd != senderFd) recipients.push_back({fd, session.transportKey});
        }
    }

    for (const auto& [fd, key] : recipients) {
        sendLine(fd, encryptForTransport(plaintext, key));
    }
}

int findClientByName(const std::string& username) {
    for (const auto& [fd, session] : clients) {
        if (session.username == username) return fd;
    }
    return -1;
}

bool usernameTaken(const std::string& username) {
    for (const auto& [_fd, session] : clients) {
        if (session.username == username) return true;
    }
    return false;
}

void sendUsage(int clientSocket) {
    sendEncrypted(
        clientSocket,
        "Commands:\n"
        "  /msg <user> <message>   Send private message\n"
        "  /who or /online         List online users\n"
        "  /history [user|public]  Show chat history\n"
        "  /help                   Show this help\n"
        "  /quit                   Disconnect");
}

void sendHistory(int clientSocket, const std::string& requester, const std::string& target) {
    std::lock_guard<std::mutex> lock(historyMutex);

    if (target.empty() || target == "public") {
        if (publicHistory.empty()) {
            sendEncrypted(clientSocket, "No public history available yet.");
            return;
        }

        std::string out = "Recent public messages:";
        for (const auto& line : publicHistory) out += "\n" + line;
        sendEncrypted(clientSocket, out);
        return;
    }

    const std::string key = privateHistoryKey(requester, target);
    auto it = privateHistory.find(key);
    if (it == privateHistory.end() || it->second.empty()) {
        sendEncrypted(clientSocket, "No private history found for that user.");
        return;
    }

    std::string out = "Recent private messages with " + target + ":";
    for (const auto& line : it->second) out += "\n" + line;
    sendEncrypted(clientSocket, out);
}

void loadUsers() {
    std::ifstream in(USER_DB_FILE);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        line = trimLineEndings(line);
        if (line.empty()) continue;
        const size_t sep = line.find(':');
        if (sep == std::string::npos) continue;
        userHashes[line.substr(0, sep)] = line.substr(sep + 1);
    }
}

void persistUsers() {
    std::ofstream out(USER_DB_FILE, std::ios::trunc);
    for (const auto& [user, hash] : userHashes) {
        out << user << ':' << hash << '\n';
    }
}

bool authenticateOrRegister(const std::string& username, const std::string& password, bool& isNewAccount) {
    isNewAccount = false;
    if (password.size() < 6) return false;

    const std::string hash = hashPassword(username, password);
    std::lock_guard<std::mutex> lock(authMutex);

    const auto it = userHashes.find(username);
    if (it == userHashes.end()) {
        userHashes[username] = hash;
        persistUsers();
        isNewAccount = true;
        return true;
    }

    return it->second == hash;
}

bool handleAuth(int clientSocket, ClientSession& outSession) {
    sendLine(clientSocket, "AUTH_REQUIRED");

    std::string authLine;
    if (!recvLine(clientSocket, authLine)) return false;

    if (authLine.rfind("AUTH ", 0) != 0) {
        sendLine(clientSocket, "AUTH_FAIL Invalid auth format. Use: AUTH <username> <password>");
        return false;
    }

    const size_t first = authLine.find(' ', 5);
    if (first == std::string::npos) {
        sendLine(clientSocket, "AUTH_FAIL Missing password.");
        return false;
    }

    const std::string username = authLine.substr(5, first - 5);
    const std::string password = authLine.substr(first + 1);
    if (username.empty() || password.empty()) {
        sendLine(clientSocket, "AUTH_FAIL Username/password cannot be empty.");
        return false;
    }

    bool isNew = false;
    if (!authenticateOrRegister(username, password, isNew)) {
        sendLine(clientSocket, "AUTH_FAIL Invalid credentials (or password too short; min 6 chars).");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        if (usernameTaken(username)) {
            sendLine(clientSocket, "AUTH_FAIL Username is already connected.");
            return false;
        }

        outSession.username = username;
        outSession.transportKey = deriveKey(username, password);
        clients[clientSocket] = outSession;
    }

    sendLine(clientSocket, std::string("AUTH_OK ") + (isNew ? "registered" : "authenticated"));
    return true;
}
}  // namespace

void handleClient(int clientSocket) {
    ClientSession session;
    if (!handleAuth(clientSocket, session)) {
        close(clientSocket);
        return;
    }

    sendEncrypted(clientSocket, "Welcome, " + session.username + "!");
    sendUsage(clientSocket);

    const std::string joinMsg = "[" + timestampNow() + "] [System] " + session.username + " joined the chat";
    {
        std::lock_guard<std::mutex> lock(historyMutex);
        pushHistory(publicHistory, joinMsg);
    }
    broadcastEncrypted(joinMsg, clientSocket);

    std::string line;
    while (recvLine(clientSocket, line)) {
        auto plaintext = decryptFromTransport(line, session.transportKey);
        if (!plaintext) {
            sendEncrypted(clientSocket, "Protocol error: expected encrypted payload.");
            continue;
        }

        const std::string msg = trimLineEndings(*plaintext);
        if (msg.empty()) continue;

        if (msg == "/quit") break;

        if (msg == "/help") {
            sendUsage(clientSocket);
            continue;
        }

        if (msg == "/who" || msg == "/online") {
            std::string userList = "Online users:";
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                for (const auto& [_fd, s] : clients) userList += "\n - " + s.username;
            }
            sendEncrypted(clientSocket, userList);
            continue;
        }

        if (msg.rfind("/history", 0) == 0) {
            std::string target;
            if (msg.size() > 9) target = trimLineEndings(msg.substr(9));
            while (!target.empty() && target.front() == ' ') target.erase(target.begin());
            sendHistory(clientSocket, session.username, target);
            continue;
        }

        if (msg.rfind("/msg ", 0) == 0) {
            const size_t split = msg.find(' ', 5);
            if (split == std::string::npos) {
                sendEncrypted(clientSocket, "Usage: /msg <user> <message>");
                continue;
            }

            const std::string target = msg.substr(5, split - 5);
            const std::string content = msg.substr(split + 1);
            if (target.empty() || content.empty()) {
                sendEncrypted(clientSocket, "Usage: /msg <user> <message>");
                continue;
            }

            int targetFd = -1;
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                targetFd = findClientByName(target);
            }

            if (targetFd == -1) {
                sendEncrypted(clientSocket, "User not found or offline.");
                continue;
            }

            const std::string ts = timestampNow();
            const std::string privMsg = "[" + ts + "] [Private] " + session.username + ": " + content;
            sendEncrypted(targetFd, privMsg);
            sendEncrypted(clientSocket, "[" + ts + "] [Private -> " + target + "] " + content);

            {
                std::lock_guard<std::mutex> lock(historyMutex);
                pushHistory(privateHistory[privateHistoryKey(session.username, target)], privMsg);
            }
            continue;
        }

        const std::string publicMsg = "[" + timestampNow() + "] " + session.username + ": " + msg;
        {
            std::lock_guard<std::mutex> lock(historyMutex);
            pushHistory(publicHistory, publicMsg);
        }
        broadcastEncrypted(publicMsg, clientSocket);
    }

    close(clientSocket);
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.erase(clientSocket);
    }

    const std::string leaveMsg = "[" + timestampNow() + "] [System] " + session.username + " left the chat";
    {
        std::lock_guard<std::mutex> lock(historyMutex);
        pushHistory(publicHistory, leaveMsg);
    }
    broadcastEncrypted(leaveMsg);
}

int main() {
    loadUsers();

    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create server socket.\n";
        return 1;
    }

    const int reuse = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "Bind failed on port " << PORT << ".\n";
        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, 16) < 0) {
        std::cerr << "Listen failed.\n";
        close(serverSocket);
        return 1;
    }

    std::cout << "MeshTalk Server running on port " << PORT << " (auth + encrypted transport enabled)...\n";

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientSize = sizeof(clientAddr);
        const int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientSize);
        if (clientSocket < 0) continue;
        std::thread(handleClient, clientSocket).detach();
    }

    close(serverSocket);
    return 0;
}
