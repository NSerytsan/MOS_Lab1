#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <time.h>

#define UNIX_SOCKET_PATH "/tmp/unix_socket"
#define BUFFER_SIZE 10000


double measure_time(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}


int create_socket(int domain) {
    int sockfd = socket(domain, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}


void connect_socket(int sockfd, const struct sockaddr *addr, socklen_t addr_len) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    if (connect(sockfd, addr, addr_len) < 0) {
        perror("Connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    printf("Connection established in %f seconds\n", measure_time(start, end));
}


void send_packets(int sockfd, int num_packets, int packet_size) {
    char buffer[packet_size];
    struct timespec send_start, send_end;
    clock_gettime(CLOCK_MONOTONIC, &send_start);

    for (int i = 0; i < num_packets; i++) {
        if (send(sockfd, buffer, packet_size, 0) < 0) {
            perror("Packet send failed");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &send_end);
    double total_send_time = measure_time(send_start, send_end);
    double packets_per_second = num_packets / total_send_time;
    double megabytes_sent = (double)(num_packets * packet_size) / (1024.0 * 1024.0);
    double mbps = megabytes_sent / total_send_time;

    printf("Total time: %f seconds\n", total_send_time);
    printf("Throughput: %f packets per second\n", packets_per_second);
    printf("Throughput: %f MB per second\n", mbps);
}


void close_socket(int sockfd) {
    struct timespec close_start, close_end;
    clock_gettime(CLOCK_MONOTONIC, &close_start);

    close(sockfd);

    clock_gettime(CLOCK_MONOTONIC, &close_end);
    printf("Socket closure time: %f seconds\n", measure_time(close_start, close_end));
}

// INET 
void inet_client(const char* ip, int port, int num_packets, int packet_size) {
    int sockfd = create_socket(AF_INET);
    struct sockaddr_in server_addr = {0};

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    connect_socket(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    send_packets(sockfd, num_packets, packet_size);
    close_socket(sockfd);
}

// UNIX
void unix_client(int num_packets, int packet_size) {
    int sockfd = create_socket(AF_UNIX);
    struct sockaddr_un server_addr = {0};

    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, UNIX_SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    connect_socket(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    send_packets(sockfd, num_packets, packet_size);
    close_socket(sockfd);
}

// 
int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <inet/unix> <ip/port/path> <num_packets> <packet_size>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_packets = atoi(argv[argc - 2]);
    int packet_size = atoi(argv[argc - 1]);

    if (strcmp(argv[1], "inet") == 0) {
        const char *ip = argv[2];
        int port = atoi(argv[3]);
        inet_client(ip, port, num_packets, packet_size);
    } else if (strcmp(argv[1], "unix") == 0) {
        unix_client(num_packets, packet_size);
    } else {
        fprintf(stderr, "Invalid socket type. Use 'inet' or 'unix'.\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
