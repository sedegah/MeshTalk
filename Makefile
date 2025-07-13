all:
	g++ ChatServer.cpp -o server -pthread
	g++ ChatClient.cpp -o client -pthread

