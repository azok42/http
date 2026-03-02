#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#define PORT 4242
#define MAX_CLIENTS 10
#define CHUNK_SIZE 512

int initConnection();
int fileExists(char *fname);
char *getFileExtension(char *file);
char* getContentType(char *path);
int getHeader(int socket, char **buffer);
int getPath(char *buffer, char **returnPath);
int sendFile(int socket, char *finalPath);
int getContentLength(char *finalPath);
int getFinalPath(char *path, char **returnPath);
int sendHeader(int socket, int code, char *type, int length);
void getDate(char *buf, size_t len);

// signal handlers
void handleSigchld(int sig);

int main()
{
	initConnection();

	return 0;
}

int initConnection() {
	int sockfd, ret, clientSocket;
	struct sockaddr_in serverAddr, cliAddr;
	socklen_t addr_size = 0;
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

	signal(SIGCHLD, handleSigchld);

	while (1) {
		clientSocket = accept(sockfd, (struct sockaddr*)&cliAddr, &addr_size);
		if (clientSocket < 0)
			exit(1);

		if ((childpid = fork()) == 0) {
			close(sockfd);

			char *headers;
			char *path;
			getHeader(clientSocket, &headers);
			getPath(headers, &path);

			char* contentType = getContentType(path);

			char *fullPath;
			getFinalPath(path, &fullPath);
			if(!fileExists(fullPath)) {
				sendHeader(clientSocket, 404, "text/html", getContentLength("www/notfound.html"));
				sendFile(clientSocket, "www/notfound.html");
			}

			sendHeader(clientSocket, 200, contentType, getContentLength(fullPath));
			sendFile(clientSocket, fullPath);
			
			free(headers);
			free(path);
			free(fullPath);

			exit(0);
		}
	}

	close(clientSocket);
	return 0;
}

char *getFileExtension(char *file) {
    char *dot = strrchr(file, '.');
    if(!dot || dot == file) return "";
    return dot + 1;
}

char* getContentType(char *path) {
	char *extension = getFileExtension(path);

	if (strcmp(extension, "json") == 0)
		return "application/json; charset=utf-8";
	else if (strcmp(extension, "svg") == 0)
		return "image/svg+xml";
	else if (strcmp(extension, "webp") == 0)
		return "image/webp";
	else if (strcmp(extension, "png") == 0)
		return "image/png";
	else if (strcmp(extension, "html") == 0)
		return "text/html; charset=utf-8";
	else if (strcmp(extension, "css") == 0)
		return "text/css; charset=utf-8";
	else if (strcmp(extension, "js") == 0)
		return "text/javascript";
	
	return "none";
}

int getPath(char *buffer, char **returnPath) {
	char *pathLocation = strstr(buffer, "/");
	char *pathEndLocation = strstr(pathLocation, " ");

	int returnPathSize = pathEndLocation - pathLocation;
	*returnPath = (char *) malloc(returnPathSize + 1);
	strncpy(*returnPath, pathLocation, returnPathSize);
	(*returnPath)[returnPathSize] = '\0';
	return 0;
}

int getHeader(int socket, char **buffer) {
	char bufferTmp[CHUNK_SIZE];
    ssize_t receivedBytes;
	*buffer = NULL;

	while ((receivedBytes = recv(socket, bufferTmp, CHUNK_SIZE - 1, 0)) > 0) {
		bufferTmp[receivedBytes] = '\0';

		if (!(*buffer)) {
			*buffer = (char *) malloc(receivedBytes + 1);
			strcpy(*buffer, bufferTmp);
		} else {
			*buffer = realloc(*buffer, strlen(*buffer) + receivedBytes + 1);
			strcat(*buffer, bufferTmp);
		}

		char *header_end = strstr(*buffer, "\r\n\r\n");
		if (header_end) {
			*header_end = '\0';
			return 0;
		}
	}

	return -1;
}

int fileExists(char *fname) {
    FILE *file;
    if ((file = fopen(fname, "r")))
    {
        fclose(file);
        return 1;
    }
    return 0;
}

int getFinalPath(char *path, char **returnPath) {
	*returnPath = (char *) malloc(strlen(path) + 4);
	sprintf(*returnPath, "www%s", path);
	return 0;
}

int getContentLength(char *finalPath) {
	FILE *fp = fopen(finalPath, "rb");
	if (!fp)
		return -1;

	fseek(fp, 0L, SEEK_END);
	int contentSize = ftell(fp);
	fclose(fp);
	return contentSize;
}

int sendFile(int socket, char *finalPath) {
	char *fData[CHUNK_SIZE];
	FILE *fp = fopen(finalPath, "rb");

	size_t nbytes = 0;
	int sent = 0;

	while ( (nbytes = fread(fData, sizeof(char), CHUNK_SIZE, fp)) > 0) {
		int offset = 0;
		while ((sent = send(socket, fData + offset, nbytes, 0)) > 0 || (sent == -1 && errno == EINTR) ) {
			if (sent <= 0)
				continue;
			
			offset += sent;
			nbytes -= sent;
		}
	}

	fclose(fp);

	return 0;
}

int sendHeader(int socket, int code, char *type, int length) {
	char *header;
	char dateBuf[30];
	getDate(dateBuf, sizeof(dateBuf));

	int headerLen = asprintf(&header,
    						"HTTP/1.1 %d\r\n"
    						"Date: %s\r\n"
    						"Content-Type: %s\r\n"
    						"Content-Length: %d\r\n"
							"Connection: close\r\n\r\n",
    						code, dateBuf, type, length);

	if (headerLen <= 0)
		return -1;

	send(socket, header, headerLen, 0);
	free(header);

	return 0;
}

void getDate(char *buf, size_t len) {
	time_t now = time(NULL);

	strftime(buf, len, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
}

void handleSigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
