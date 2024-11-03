#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <poll.h>

#define UNIX_SOCKET_PATH "/tmp/unix_socket"
#define BUFFER_SIZE 10000

// Function to calculate elapsed time in seconds
double elapsed_time(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

// Socket setup function for INET and UNIX types
int setup_socket(int domain, int port, const char *path, int non_blocking) {
    int socket_fd = socket(domain, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    if (non_blocking) fcntl(socket_fd, F_SETFL, O_NONBLOCK);

    if (domain == AF_INET) {
        struct sockaddr_in addr_in;
        addr_in.sin_family = AF_INET;
        addr_in.sin_addr.s_addr = INADDR_ANY;
        addr_in.sin_port = htons(port);
        if (bind(socket_fd, (struct sockaddr*)&addr_in, sizeof(addr_in)) < 0) {
            perror("INET bind failed");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }
    } else if (domain == AF_UNIX) {
        struct sockaddr_un addr_un;
        addr_un.sun_family = AF_UNIX;
        strncpy(addr_un.sun_path, path, sizeof(addr_un.sun_path) - 1);
        unlink(path);
        if (bind(socket_fd, (struct sockaddr*)&addr_un, sizeof(addr_un)) < 0) {
            perror("UNIX bind failed");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }
    }
    if (listen(socket_fd, 5) < 0) {
        perror("Listen failed");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }
    return socket_fd;
}

// Function to accept and handle client connections
void handle_connection(int server_fd, int async_mode) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    char buffer[BUFFER_SIZE];
    socklen_t addr_len;
    struct sockaddr_storage client_addr;
    addr_len = sizeof(client_addr);

    while (1) {
        int client_fd;
        if (async_mode) {
            struct pollfd poll_fd = {.fd = server_fd, .events = POLLIN};
            int poll_status = poll(&poll_fd, 1, 1000);

            if (poll_status == -1) {
                perror("Polling error");
                break;
            } else if (poll_status > 0 && (poll_fd.revents & POLLIN)) {
                client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
            } else {
                continue;
            }
        } else {
            client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        }

        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror("Accept failed");
            continue;
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        printf("Connection accepted in %f seconds\n", elapsed_time(start, end));

        while (read(client_fd, buffer, sizeof(buffer)) > 0);
        close(client_fd);
    }
}

// Main function to set up server and call connection handling
int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <inet/unix> <port/path> <non-blocking (0/1)> <async (0/1)>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int non_blocking = atoi(argv[3]);
    int async_mode = atoi(argv[4]);
    int server_fd;

    if (strcmp(argv[1], "inet") == 0) {
        int port = atoi(argv[2]);
        server_fd = setup_socket(AF_INET, port, NULL, non_blocking);
        printf("INET server listening on port %d...\n", port);
    } else if (strcmp(argv[1], "unix") == 0) {
        server_fd = setup_socket(AF_UNIX, 0, UNIX_SOCKET_PATH, non_blocking);
        printf("UNIX server listening at %s...\n", UNIX_SOCKET_PATH);
    } else {
        fprintf(stderr, "Invalid type. Use 'inet' or 'unix'.\n");
        return EXIT_FAILURE;
    }

    handle_connection(server_fd, async_mode);
    close(server_fd);
    return 0;
}
