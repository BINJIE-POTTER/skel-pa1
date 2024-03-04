#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

#include <pthread.h>
#include <errno.h>

#define BUFFER_SIZE 1024 // Adjust based on expected MTU

char* 
getIPv4(char* hostname) {

    struct addrinfo hints, *res, *p;
    int status;
    char* ipstr = malloc(INET_ADDRSTRLEN); // Allocate memory for the IP string

    if (ipstr == NULL) {
        fprintf(stderr, "Malloc failed\n");
        return NULL;
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        free(ipstr);
        return NULL;
    }

    // Loop through all the results and convert the first one to an IP string
    for(p = res; p != NULL; p = p->ai_next) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
        inet_ntop(p->ai_family, &(ipv4->sin_addr), ipstr, INET_ADDRSTRLEN);
        break; // Break after the first IP address is found
    }

    freeaddrinfo(res); // Free the linked list
    return ipstr; // Return the first IP address found

}

void 
rsend(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    
    int sockfd;
    struct sockaddr_in servaddr;
    FILE *fp;
    char buffer[BUFFER_SIZE];
    char* ipv4;

    printf("Hostname: %s\n", hostname);
    printf("UDP Port: %hu\n", hostUDPport);
    printf("File to send: %s\n", filename);
    printf("Bytes to transfer: %llu\n", bytesToTransfer);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    int bufferSize = 2 * 1024 * 1024; // Example: set buffer to 2 MB
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(hostUDPport);

    ipv4 = getIPv4(hostname);
    if(!ipv4) {
        perror("get ipv4 address failed");
        exit(EXIT_FAILURE);
    }
    printf("IPv4: %s\n", ipv4);

    int addrResult = inet_pton(AF_INET, ipv4, &servaddr.sin_addr);
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

    }

    for(int i = 0; i < 10; i++) sendto(sockfd, NULL, 0, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));

    printf("Packets sent: %zu\n", packetsSent);

    fclose(fp);
    close(sockfd);

}

int 
main(int argc, char** argv) {

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