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

#define HEADER_END "\r\n"

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

// errors
#define SOCKET_ERROR -1
#define MALLOC_ERROR -2
#define HEADER_SYNTAX_ERROR -3

int init_sockets();
int main_loop(int server_socket);

int handle_request(int socket);
ssize_t receive_header(int socket, char **header);
ssize_t get_request_line(char *header, ssize_t header_length, char **request_line);

int log_message(char *type, char *msg);

void init_signals();
void handle_sigchld(int sig);
void handle_sigsegv(int sig);

void get_date(char *buf, size_t len);

int main(int argc, char **argv)
{
    init_signals();

    int server_socket = init_sockets();

    main_loop(server_socket);

    return 1;
}

void init_signals() {
    signal(SIGCHLD, handle_sigchld);
    signal(SIGSEGV, handle_sigsegv);
}

void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void handle_sigsegv(int sig) {
    log_message(ERROR, "Received SEGV signal");
    exit(sig);
}

int init_sockets()
{
    int server_socket, ret;
    struct sockaddr_in server_addr;
    socklen_t addr_size = 0;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        log_message(ERROR, "Error in socket creation.");
        exit(SOCKET_ERROR);
    }
    log_message(INFO, "Server Socket is created.");

    int reuse = 1;
    int result = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse, sizeof(reuse));
    if (result < 0) {
        log_message(ERROR, "Failed to set socket option SO_REUSEADDR");
        exit(SOCKET_ERROR);
    }

    memset(&server_addr, '\0', sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        log_message(ERROR, "Error in binding.");
        exit(SOCKET_ERROR);
    }

    if (listen(server_socket, MAX_CLIENTS) != 0) {
        log_message(ERROR, "Error at starting to listen");
        exit(SOCKET_ERROR);
    }
    log_message(INFO, "Listening...\n");


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
        if (client_socket < 0) {
            log_message(ERROR, "Failed to accept connection");
            exit(SOCKET_ERROR);
        }

        if ((child_pid = fork()) == 0) {
            close(server_socket);

            handle_request(client_socket);

            close(client_socket);
            exit(0);
        } else if (child_pid == -1) {
            log_message(ERROR, "Failed to fork");
        }

        close(client_socket);
    }
}

void get_date(char *buf, size_t len) {
    time_t now = time(NULL);

    strftime(buf, len, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
}

int log_message(char *type, char *msg)
{
    char datetime_buffer[30];
    get_date(datetime_buffer, sizeof datetime_buffer);

    if (LOG_TO_STDOUT)
        printf("%s %s[%s]%s %s\n", type, BLU, datetime_buffer, RESET, msg);

    return 0;
}

int handle_request(int socket)
{
    char *header;
    ssize_t header_length = receive_header(socket, &header);

    char *request_line;
    ssize_t request_line_size = get_request_line(header, header_length, &request_line);

    free(header);
    free(request_line);
    return 0;
}

ssize_t receive_header(int socket, char **header)
{
    char buffer[CHUNK_SIZE];
    ssize_t received_bytes, header_size = 0;
    *header = NULL;

    while ((received_bytes = recv(socket, buffer, CHUNK_SIZE-1, 0)) > 0) {
        buffer[received_bytes] = '\0';
        header_size += received_bytes;

        // check if buffer is not already malloc'd
		if (!(*header)) {
			*header = (char *) malloc(received_bytes + 1);
            if (!header)
                return MALLOC_ERROR;

			strcpy(*header, buffer);
		} else {
			*header = realloc(*header, strlen(*header) + received_bytes + 1);
            if (!*header)
                return MALLOC_ERROR;

			strcat(*header, buffer);
		}

		char *header_end = strstr(*header, "\r\n\r\n");
		if (header_end) {
            *header_end = '\0';
			return header_size;
        }
    }

    return -1;
}

ssize_t get_request_line(char *header, ssize_t header_length, char **request_line)
{
    char *request_line_end = strstr(header, HEADER_END);
    if (!request_line_end)
        return HEADER_SYNTAX_ERROR;

    size_t request_line_size = request_line_end - header;

    *request_line = (char*) malloc(request_line_size + 1);
    if (!*request_line)
        return MALLOC_ERROR;

    strncpy(*request_line, header, request_line_size);
    (*request_line)[request_line_size] = '\0';

    return request_line_size;
}
