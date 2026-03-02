all:
	g++ ChatServer.cpp -o server -pthread
	g++ ChatClient.cpp -o client -pthread

backend-check:
	node --check cloudflare-backend/src/index.js

setup:
	./scripts/setup.sh
