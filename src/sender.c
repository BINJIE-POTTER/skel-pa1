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

void rsend(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    
    int sockfd;
    struct sockaddr_in servaddr;
    FILE *fp;
    char buffer[BUFFER_SIZE];
    struct timespec req = {0}; // For introducing delay
    
    req.tv_sec = 0;
    req.tv_nsec = 10 * 1000000L; // 10 milliseconds delay

    printf("Hostname: %s\n", hostname);
    printf("UDP Port: %hu\n", hostUDPport);
    printf("File to send: %s\n", filename);
    printf("Bytes to transfer: %llu\n", bytesToTransfer);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(hostUDPport);

    int addrResult = inet_pton(AF_INET, hostname, &servaddr.sin_addr);
    if (addrResult <= 0) {
        perror("addr parse failed");
        exit(EXIT_FAILURE);
    }

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    printf("Succeed to open the file!\n");

    unsigned long long int sentBytes = 0;
    size_t packetsSent = 0; // For diagnostic

    unsigned long long int sentBytes = 0;
    while (sentBytes < bytesToTransfer && fread(buffer, 1, BUFFER_SIZE, fp) > 0) {

        ssize_t toSend = (bytesToTransfer - sentBytes < BUFFER_SIZE) ? bytesToTransfer - sentBytes : BUFFER_SIZE;

        ssize_t sent = sendto(sockfd, buffer, toSend, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
        if (sent < 0) {
            perror("Failed to send file");
            break;
        } 
        
        sentBytes += sent;
        packetsSent++;
        nanosleep(&req, (struct timespec *)NULL); // Delay between sends

    }

    sendto(sockfd, NULL, 0, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));

    printf("Packets sent: %zu\n", packetsSent);

    fclose(fp);
    close(sockfd);

}

int main(int argc, char** argv) {

    if (argc != 5) {
        fprintf(stderr, "Usage: %s <hostname> <UDP port> <file to send> <bytes to transfer>\n", argv[0]);
        return 1;
    }

    char* hostname = argv[1];
    unsigned short int hostUDPport = (unsigned short int)atoi(argv[2]);
    char* filename = argv[3];
    unsigned long long int bytesToTransfer = atoll(argv[4]);

    rsend(hostname, hostUDPport, filename, bytesToTransfer);

    printf("MAIN END\n");

    return 0;
}