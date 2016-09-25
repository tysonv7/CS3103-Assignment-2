#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>

using namespace std;

const int maxMessageReceiveSize = 140 * 1024;

string generateHttpRequest(char *path) {
	string requestMessage = "GET ";
	requestMessage.append(path); //path is the hostURL
	requestMessage.append(" HTTP/1.0\r\n");
	requestMessage.append("Host: ");
	requestMessage.append("\r\n");
	requestMessage.append("Accept: text/*, */*;q=0/9\r\n");
	requestMessage.append("Connection: close\r\n\r\n");
	return requestMessage;
}

int connect(char* website) {
	int sock;
	struct hostent* host;
	struct sockaddr_in server_addr;
	
	host = gethostbyname(website);
	
	cout << "Try to visit: " << website << endl;
	
	//Creating a socket Structure
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		cout << "Error in socket creation" << endl;
		exit(1);
	}
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(80);
	server_addr.sin_addr = *((struct in_addr *)host -> h_addr);
	bzero(&(server_addr.sin_zero), 8);
	
	//Connect to HTTP Server Port 80
	if (connect(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
		cout << "Error in connecting to HTTP Port 80" << endl;
		exit(1);
	}
	
	cout << "Connected to " << inet_ntoa(server_addr.sin_addr) << " " << ntohs(server_addr.sin_port) << endl;
	
	string requestMessage = generateHttpRequest(website);
	
	int status = send(sock, requestMessage.c_str(), requestMessage.size(), 0);
	cout << "Request: " << requestMessage << endl;
	
	char buf[maxMessageReceiveSize];
	
	string messageReceived = "";
	
	while (status !=0) {
		memset(buf, 0, maxMessageReceiveSize);
		status = recv(sock, buf, maxMessageReceiveSize, 0);
		messageReceived.append(buf);
	}
	cout << "Received: " << messageReceived << endl;
	
	return 0;
}

int main(int argc, char* argv[]) {
	connect(argv[1]);
}
