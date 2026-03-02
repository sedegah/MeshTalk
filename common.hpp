// common.hpp
#pragma once

#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <vector>

#define PORT 9002
#define BUFFER_SIZE 1024
#define HISTORY_LIMIT 100
#define USER_DB_FILE "users.db"

inline std::string trimLineEndings(std::string input) {
    input.erase(std::remove(input.begin(), input.end(), '\r'), input.end());
    input.erase(std::remove(input.begin(), input.end(), '\n'), input.end());
    return input;
}

inline std::string timestampNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

inline bool sendAll(int socketFd, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        const ssize_t n = send(socketFd, data + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

inline bool sendLine(int socketFd, const std::string& line) {
    return sendAll(socketFd, line.c_str(), line.size()) && sendAll(socketFd, "\n", 1);
}

inline bool recvLine(int socketFd, std::string& out) {
    out.clear();
    char ch = '\0';
    while (true) {
        const ssize_t n = recv(socketFd, &ch, 1, 0);
        if (n <= 0) return false;
        if (ch == '\n') break;
        out.push_back(ch);
        if (out.size() >= 8192) return false;
    }
    out = trimLineEndings(out);
    return true;
}

inline std::string toHex(const std::string& input) {
    static const char* lut = "0123456789abcdef";
    std::string out;
    out.reserve(input.size() * 2);
    for (unsigned char c : input) {
        out.push_back(lut[c >> 4]);
        out.push_back(lut[c & 0x0F]);
    }
    return out;
}

inline std::optional<std::string> fromHex(const std::string& input) {
    auto hexVal = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };

    if (input.size() % 2 != 0) return std::nullopt;
    std::string out;
    out.reserve(input.size() / 2);

    for (size_t i = 0; i < input.size(); i += 2) {
        const int hi = hexVal(input[i]);
        const int lo = hexVal(input[i + 1]);
        if (hi < 0 || lo < 0) return std::nullopt;
        out.push_back(static_cast<char>((hi << 4) | lo));
    }

    return out;
}

inline uint64_t deriveKey(const std::string& username, const std::string& password) {
    const std::string seed = username + ":" + password + ":meshtalk-v2";
    return static_cast<uint64_t>(std::hash<std::string>{}(seed));
}

inline std::string xorCipher(const std::string& input, uint64_t key) {
    if (input.empty()) return input;
    std::string out = input;
    uint64_t state = key ^ 0x9E3779B97F4A7C15ULL;
    for (size_t i = 0; i < out.size(); ++i) {
        state = (state * 2862933555777941757ULL) + 3037000493ULL;
        out[i] = static_cast<char>(static_cast<unsigned char>(out[i]) ^ static_cast<unsigned char>((state >> 24) & 0xFF));
    }
    return out;
}

inline std::string encryptForTransport(const std::string& plaintext, uint64_t key) {
    return "ENC:" + toHex(xorCipher(plaintext, key));
}

inline std::optional<std::string> decryptFromTransport(const std::string& payload, uint64_t key) {
    if (payload.rfind("ENC:", 0) != 0) return std::nullopt;
    auto binary = fromHex(payload.substr(4));
    if (!binary) return std::nullopt;
    return xorCipher(*binary, key);
}

inline std::string hashPassword(const std::string& username, const std::string& password) {
    const std::string salted = "meshtalk-auth::" + username + "::" + password;
    const auto h1 = static_cast<uint64_t>(std::hash<std::string>{}(salted));
    const auto h2 = static_cast<uint64_t>(std::hash<std::string>{}(salted + "::pepper"));
    std::ostringstream oss;
    oss << std::hex << h1 << h2;
    return oss.str();
}
