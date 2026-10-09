#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

#define PORT 4242
#define CHUNK_SIZE 512
#define MAX_CLIENTS 10

#define LOG_TO_STDOUT 1

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define RESET "\x1B[0m"

#define INFO "INFO"
#define SUCCESS "\x1B[32mSUCCESS\x1B[0m"
#define ERROR "\x1B[31mERROR\x1B[0m"
#define DEBUG "\x1B[33mDEBUG\x1B[0m"

int init_sockets();
int main_loop(int server_socket);

int handle_request(int socket);

int log_message(char *type, char *msg);

void init_signals();
void handle_sigchld(int sig);

void getDate(char *buf, size_t len);

int main(int argc, char **argv)
{
    init_signals();

    int server_socket = init_sockets();

    main_loop(server_socket);

    return 1;
}

void init_signals() {
    signal(SIGCHLD, handle_sigchld);
}

void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int init_sockets()
{
    int server_socket, ret;
    struct sockaddr_in server_addr;
    socklen_t addr_size = 0;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        printf("Error in socket creation.\n");
        exit(1);
    }
    printf("Server Socket is created.\n");

    int reuse = 1;
    int result = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse, sizeof(reuse));
    if (result < 0) {
        perror("ERROR SO_REUSEADDR:");
    }

    memset(&server_addr, '\0', sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        printf("Error in binding.\n");
        exit(1);
    }

    if (listen(server_socket, MAX_CLIENTS) != 0) {
        printf("Error at starting to listen\n");
        exit(1);
    }
    printf("Listening...\n\n");

    return server_socket;
}


int main_loop(int server_socket)
{
    int client_socket;
    struct sockaddr_in client_address;
    socklen_t addr_size = 0;
    pid_t child_pid;

    while(1) {
		client_socket = accept(server_socket, (struct sockaddr*)&client_address, &addr_size);
		if (client_socket < 0)
			exit(1);

		if ((child_pid = fork()) == 0) {
            close(server_socket);
        
            handle_request(client_socket);

            close(client_socket);
            exit(0);
        }

        close(client_socket);
    }
}

void getDate(char *buf, size_t len) {
	time_t now = time(NULL);

	strftime(buf, len, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
}

int log_message(char *type_str, char *msg)
{
    char datetime_buffer[30];
    getDate(datetime_buffer, sizeof datetime_buffer);

    if (LOG_TO_STDOUT)
        printf("%s %s[%s]%s %s\n", type_str, BLU, datetime_buffer, RESET, msg);

    return 0;
}

int handle_request(int socket)
{
    return 0;
}
