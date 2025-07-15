# MeshTalk

**MeshTalk** is a terminal-based local network chat system written in C++. It enables multiple users to communicate in real time over a TCP/IP network using a client-server model. MeshTalk includes support for usernames, public chat, private messaging, and basic server-side user management.

---

## Features

- Multithreaded chat server (handles multiple clients concurrently)
- Unique usernames per session
- Real-time public chat
- Private messaging: `/msg <username> <message>`
- View online users: `/who`
- Clean disconnection: `/quit`
- Fully terminal-based — no external libraries required
- Supports LAN connections (clients can connect from other devices on the same network)

---

## Requirements

- C++17 or higher
- UNIX/Linux system with POSIX sockets (e.g., Ubuntu, macOS)
- g++ compiler
- `make` (optional but recommended)

---

## File Structure

```

MeshTalk/
├── ChatServer.cpp      # Multithreaded TCP server
├── ChatClient.cpp      # Interactive client
├── common.hpp          # Shared headers, constants, and includes
├── Makefile            # Build automation
└── README.md           # Documentation

````

---

## Build Instructions

Clone the repository and compile using the `Makefile`:

```bash
git clone https://github.com/sedegah/MeshTalk.git
cd MeshTalk
make
````

This will generate two executables:

* `server`: Run this to start the chat server
* `client`: Launch this to join the chat as a user

---

## How to Use

### Start the Server

```bash
./server
```

The server listens on port `9002` by default.

### Connect as a Client

```bash
./client <server-ip-address>
```

For local testing, use:

```bash
./client 127.0.0.1
```

You will be prompted to enter a **username**. After that, you can chat with others who are connected.

---

## Available Commands

| Command                 | Description                                    |
| ----------------------- | ---------------------------------------------- |
| `/who`                  | Lists all currently connected usernames        |
| `/msg <user> <message>` | Sends a private message to `<user>`            |
| `/quit`                 | Disconnects from the chat and exits the client |

---

## Example Usage

**Terminal 1 – Run Server**

```bash
./server
```

**Terminal 2 – Client A**

```bash
./client 127.0.0.1
Enter username: alice
> Hello everyone!
```

**Terminal 3 – Client B**

```bash
./client 127.0.0.1
Enter username: bob
> /who
> /msg alice Hey Alice, how are you?
```

---

## Security & Limitations

* Currently supports unencrypted TCP only (no TLS/SSL)
* No authentication or persistent session tracking
* Not designed for public internet use — LAN only

---

## Possible Enhancements

* Add message encryption (TLS or simple XOR)
* Support chat rooms or channels
* Add logging to file or database
* Implement a GUI with Qt or ncurses
* Dockerize for easier deployment

---

## License

This project is licensed under the MIT License. See the `LICENSE` file for more details.

---

## Author

Created by Kimathi Sedegah — Computer Science student passionate about networks and systems programming.

Feel free to fork or contribute!

