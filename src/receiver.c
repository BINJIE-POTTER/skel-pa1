#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <pthread.h>
#include <errno.h>

#define BUFFER_SIZE 1024 // Adjust based on expected MTU

void rrecv(unsigned short int myUDPport, char* destinationFile, unsigned long long int writeRate) {
    
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[BUFFER_SIZE];
    FILE *fp;
    struct timespec sleepDuration = {0, 0};

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    int bufferSize = 2 * 1024 * 1024; // Example: set buffer to 2 MB
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(myUDPport);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    fp = fopen(destinationFile, "wb");
    if (fp == NULL) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    // Calculate sleep duration if writeRate is specified
    if (writeRate > 0) {
        double bytesPerNanoSecond = writeRate / 1000000000.0;
        sleepDuration.tv_nsec = (long)(BUFFER_SIZE / bytesPerNanoSecond);
    }

    unsigned long long int receivedBytes = 0; // Track received bytes
    size_t packetsReceived = 0; // For diagnostic

    socklen_t len = sizeof(cliaddr);
    ssize_t n;
    while (1) {

        n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &cliaddr, &len);

        if (n == -1) {
            perror("recvfrom failed");
            break;
        } else if (n == 0) {
            printf("Termination packet received. Ending reception.\n");
            break;
        }

        packetsReceived++;
        receivedBytes += n;

        if (fwrite(buffer, 1, n, fp) != n) {
            perror("Failed to write to file");
            break;
        }
        fflush(fp);

        if (writeRate > 0) {
            nanosleep(&sleepDuration, NULL);
        }

    }

    printf("Total received bytes: %llu\n", receivedBytes); // Print received bytes for diagnostics
    printf("Packets received: %zu\n", packetsReceived);

    fclose(fp);
    close(sockfd);

}

int main(int argc, char** argv) {

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <UDP port> <destination file> <write rate>\n", argv[0]);
        return 1;
    }

    unsigned short int udpPort = (unsigned short int)atoi(argv[1]);
    char* destinationFile = argv[2];
    unsigned long long int writeRate = atoll(argv[3]);

    rrecv(udpPort, destinationFile, writeRate);

    return 0;

}