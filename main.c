#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#define PORT 8080
#define MAX_CLIENTS 10

int initConnection();

int main()
{
	initConnection();

	return 0;
}

int initConnection() {
	int sockfd, ret, clientSocket;
	struct sockaddr_in serverAddr, cliAddr;
	socklen_t addr_size;
	pid_t childpid;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("Error in connection.\n");
		exit(1);
	}
	printf("Server Socket is created.\n");

	int reuse = 1;
	int result = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse, sizeof(reuse));
	if ( result < 0 ) {
		perror("ERROR SO_REUSEADDR:");
	}

	memset(&serverAddr, '\0', sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);

	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	ret = bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	if (ret < 0) {
		printf("Error in binding.\n");
		exit(1);
	}

	if (listen(sockfd, MAX_CLIENTS) == 0)
		printf("Listening...\n\n");

	int cnt = 0;
	while (1) {
		clientSocket = accept(sockfd, (struct sockaddr*)&cliAddr, &addr_size);
		if (clientSocket < 0)
			exit(1);

		if ((childpid = fork()) == 0) {
			close(sockfd);

			char wbuff[100] = "HTTP/2 200";
			send(clientSocket, wbuff, sizeof(wbuff), 0);
		}
	}

	close(clientSocket);
	return 0;
}

int constructHeader(char &header, int code, string type, int length) {
	strcat(header, "HTTP/1 ");

	return 0;
}
