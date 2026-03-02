# MeshTalk Network v2 – System Documentation

## 1. Project Overview

MeshTalk Network v2 is a peer-to-peer encrypted messaging system built on a console/CLI interface, supporting:

- End-to-end encrypted messaging
- File transfers
- Presence detection
- Offline message syncing
- Multi-device login
- Optional bootstrap server for peer discovery

This system focuses on security, P2P networking, and developer-oriented features.

## 2. Goals & Objectives

- **Secure Messaging:** Ensure all messages/files are E2EE.
- **P2P Architecture:** Minimize server dependency; peers communicate directly.
- **User Identity:** Cryptographically verify all users.
- **Offline Support:** Messages queue for offline users and sync when online.
- **Developer-Friendly:** CLI interface with commands, logs, and extensibility.
- **Scalable:** Can handle multiple peers, file transfers, and group chat.

## 3. System Architecture

### 3.1 High-Level Components

| Component | Description |
| --- | --- |
| MeshTalk Node | Runs on each user’s machine. Handles messaging, encryption, CLI, file transfer, and offline queuing. |
| Bootstrap Server | Hosted on Cloudflare Workers. Handles: user registration, peer discovery, optional multi-device key storage. |
| Cloud Storage (optional) | Cloudflare R2/KV: stores encrypted private keys for multi-device login. |
| Peer Connections | TCP/UDP sockets between nodes for direct communication. |
| Local Storage | SQLite or JSON: stores chat history, queued messages, and peer info locally. |

### 3.2 Architecture Diagram

```text
          +----------------+
          | Cloudflare     |
          | Workers KV/R2  |
          +--------+-------+
                   |
       Peer Discovery & Key Storage
                   |
      +------------+------------+
      |                         |
+-----v-----+             +-----v-----+
| MeshTalk  |             | MeshTalk  |
| Node A    | <---------> | Node B    |
| CLI + P2P |   Direct    | CLI + P2P |
+-----------+   TCP/UDP   +-----------+
```

## 4. Modules & Responsibilities

### 4.1 User & Auth Module

- Generate RSA/ECC key pair on first login.
- Encrypt private key with password.
- Login decrypts the private key.
- Optional multi-device login via cloud storage.

**Data Structure:**

```json
{
  "username": "Alice",
  "public_key": "<RSA public key>",
  "private_key_encrypted": "<AES encrypted private key>",
  "last_online": "2026-03-02T14:00Z"
}
```

### 4.2 Networking & P2P Module

- TCP/UDP sockets for direct communication.
- Peer discovery via:
  - Local LAN broadcast, or
  - Cloudflare Workers bootstrap server.
- Handles connecting, disconnecting, and reconnections.

**Data Structure:**

```json
{
  "peer_username": "Bob",
  "ip": "192.168.1.5",
  "port": 5555,
  "public_key": "<Bob's public key>",
  "status": "online"
}
```

### 4.3 Messaging Module

- Sends text messages and files to peers.
- Encrypt messages with recipient’s public key.
- Sign messages with sender’s private key.
- Queue messages for offline peers.
- Store chat history locally.

**Message Format:**

```json
{
  "sender": "Alice",
  "recipient": "Bob",
  "timestamp": "2026-03-02T14:05Z",
  "payload": "<AES encrypted message>",
  "signature": "<signed hash of message>"
}
```

### 4.4 File Transfer Module

- Chunked transfer (e.g., 4 KB chunks).
- Each chunk encrypted and signed.
- Reassemble on the recipient side.
- Optional progress tracking in CLI.

### 4.5 Presence Module

- Heartbeat messages every 10–30 seconds.
- Status: `online`, `offline`, `idle`.
- Display peer list in CLI:

```text
[Online] Alice
[Offline] Bob
[Idle] Charlie
```

### 4.6 CLI / User Interface Module

**Commands:**

```text
/msg <user> <message>       # Send message
/sendfile <user> <file>     # Send file
/online                     # List online users
/history <user>             # View chat history
/exit                       # Quit
```

- Colored output, notifications, timestamps.

## 5. Data Flow

### 5.1 Login Flow

1. User enters username + password.
2. Decrypt private key locally (or fetch from Cloudflare R2 for multi-device login).
3. Connect to bootstrap server and register online status + public key.
4. Fetch list of online peers from server.

### 5.2 Sending a Message

1. User types `/msg Bob "Hello"`.
2. Node fetches Bob’s public key.
3. Encrypt message with AES; encrypt AES key with Bob’s public key.
4. Sign message with sender’s private key.
5. Send via direct TCP/UDP socket.
6. Recipient verifies signature and decrypts message.

### 5.3 Offline Message Handling

If Bob is offline:

- Queue message locally or store temporarily on bootstrap server.
- Retry sending when Bob comes online.

### 5.4 File Transfer

Similar to message flow but chunked and progress tracked.

- Each chunk encrypted + signed.

## 6. Tech Stack

| Layer | Tech/Library |
| --- | --- |
| Programming Language | C++ (MeshTalk base) / Python alternative |
| Networking | `boost::asio` (C++) or `asyncio` + `socket` (Python) |
| Encryption | OpenSSL (C++) / `cryptography` (Python) |
| Storage | SQLite / JSON for local chat history; Cloudflare KV/R2 for multi-device login |
| CLI UI | `ncurses` (C++) / `rich` (Python) |
| Cloud Backend | Cloudflare Workers for bootstrap + peer discovery |

## 7. Security Considerations

- End-to-End Encryption for messages and files.
- Private key encryption with strong passwords.
- Signature verification to prevent impersonation.
- Optional NAT traversal via STUN/TURN for non-LAN environments.
- Offline queue encryption at rest.

## 8. Development Roadmap

- **Phase 1 – Base CLI Chat**
  - Text messaging P2P over TCP/UDP
  - Local storage of chat history
- **Phase 2 – Encryption**
  - RSA/ECC key pair generation
  - AES message encryption + signature verification
- **Phase 3 – File Transfer**
  - Chunked, encrypted file transfer
  - Progress tracking
- **Phase 4 – Presence & Offline Messages**
  - Heartbeat-based online/offline status
  - Offline message queue and sync
- **Phase 5 – Bootstrap Server**
  - Cloudflare Workers for peer discovery
  - Optional multi-device private key storage
- **Phase 6 – CLI Enhancements**
  - Commands, colors, notifications
  - Logs and debugging tools

## 9. Example CLI Session

```text
> /login Alice
Welcome, Alice!
Fetching online peers...
[Online] Bob
[Offline] Charlie

> /msg Bob "Hey Bob, are you online?"
Message sent ✔

> /sendfile Bob report.pdf
Sending file: 15% 30% 45% 60% 75% 90% 100% ✔
File delivered securely ✔

> /online
[Online] Bob
[Offline] Charlie
```

This document captures architecture, modules, data flow, CLI commands, storage, encryption strategy, and phased implementation guidance for MeshTalk Network v2.
