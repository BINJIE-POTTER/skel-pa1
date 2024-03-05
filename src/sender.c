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

#include <math.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024

typedef struct {
    char* hostname;
    unsigned short int hostUDPport;
    char* filename;
    unsigned long long int bytesToTransfer;
} RSendArgs;

typedef struct {
    unsigned short int hostUDPport;
} RRecvACKArgs;

typedef struct {
    unsigned int index;
    char data[BUFFER_SIZE];
} Packet;

typedef struct {
    unsigned int index; // -1 means control package
    size_t packetNum; // Total number of packets
} InfoPacket;

pthread_mutex_t lock;
bool *array;
size_t ARRAY_SIZE;
volatile bool receiverFinished = false;

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

void*
rrecvACK(void* args) {

    RRecvACKArgs* recvArgs = (RRecvACKArgs*)args;
    unsigned short int hostUDPport = recvArgs->hostUDPport;

    int sockfd;
    struct sockaddr_in recvaddr, senderaddr;
    socklen_t senderaddrlen = sizeof(senderaddr);
    unsigned int ack;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    int bufferSize = 2 * 1024 * 1024;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize));

    memset(&recvaddr, 0, sizeof(recvaddr));
    recvaddr.sin_family = AF_INET;
    recvaddr.sin_addr.s_addr = INADDR_ANY;
    recvaddr.sin_port = htons(hostUDPport);

    if (bind(sockfd, (const struct sockaddr *)&recvaddr, sizeof(recvaddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("start receiving ack.\n");

    ssize_t n;
    while (1) {

        printf("receiving...\n");

        n = recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&senderaddr, &senderaddrlen);
        if (n < 0) {
            perror("recvfrom failed");
            exit(EXIT_FAILURE);
        }

        if (ack == -1) break;
        else printf("ACK received: %u", ack);

        pthread_mutex_lock(&lock);

        array[ack] = true;

        pthread_mutex_unlock(&lock);

    }

    receiverFinished = true; 

    printf("All packets have been received.\n");

    close(sockfd);

    return NULL;

}


void 
rsend(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    
    int sockfd;
    struct sockaddr_in servaddr;
    FILE *fp;
    char* ipv4;
    ssize_t sent;

    printf("Hostname: %s\n", hostname);
    printf("UDP Port: %hu\n", hostUDPport);
    printf("File to send: %s\n", filename);
    printf("Bytes to transfer: %llu\n", bytesToTransfer);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    int bufferSize = 2 * 1024 * 1024;
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

    InfoPacket infoPacket;
    infoPacket.index = -1;
    infoPacket.packetNum = (size_t) ceil((double)bytesToTransfer / BUFFER_SIZE);
    for (int i = 0; i < 20; i++) {

        sent = sendto(sockfd, &infoPacket, sizeof(infoPacket), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
        if (sent < 0) {
            perror("Failed to send file");
            break;
        } 

    }

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    printf("Succeed to open the file!\n");

    while (!receiverFinished) {

        for (unsigned int index = 0; index < ARRAY_SIZE; ++index) {

            //pthread_mutex_lock(&lock);

            if (!array[index]) {

                printf("index: %u\n", index);

                Packet packet;
                packet.index = index;

                off_t position = (off_t)index * BUFFER_SIZE;
                fseek(fp, position, SEEK_SET);

                if (fread(packet.data, 1, BUFFER_SIZE, fp) <= 0) {
                    perror("Failed to read file");
                    break;
                }

                sent = sendto(sockfd, &packet, sizeof(packet), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
                if (sent < 0) {
                    perror("Failed to send file");
                    break;
                }

            }

            //pthread_mutex_unlock(&lock);

        }

    }

    printf("rsend finished\n");

    fclose(fp);
    close(sockfd);

}

void* 
rsendHelper(void* args) {

    RSendArgs* sendArgs = (RSendArgs*)args;
    rsend(sendArgs->hostname, sendArgs->hostUDPport, sendArgs->filename, sendArgs->bytesToTransfer);
    return NULL;

}

int 
main(int argc, char** argv) {

    if (argc != 5) {
        fprintf(stderr, "Usage: %s <hostname> <UDP port> <file to send> <bytes to transfer>\n", argv[0]);
        return 1;
    }

    pthread_mutex_init(&lock, NULL);

    ARRAY_SIZE = (size_t) ceil((double)atoll(argv[4]) / BUFFER_SIZE);
    array = malloc(ARRAY_SIZE * sizeof(bool));
    if (array == NULL) {
        fprintf(stderr, "Failed to allocate memory for the array.\n");
        return EXIT_FAILURE;
    }
    for (int i = 0; i < ARRAY_SIZE; ++i) {
        array[i] = false;
    }

    printf("Packets going to send: %zu\n", ARRAY_SIZE);

    RSendArgs sendArgs = {
        .hostname = argv[1],
        .hostUDPport = (unsigned short int)atoi(argv[2]),
        .filename = argv[3],
        .bytesToTransfer = atoll(argv[4])
    };

    RRecvACKArgs recvACKArgs = { .hostUDPport = (unsigned short int)atoi(argv[2]) };

    pthread_t sendThread, recvACKThread;
    pthread_create(&sendThread, NULL, rsendHelper, &sendArgs);
    pthread_create(&recvACKThread, NULL, rrecvACK, &recvACKArgs);

    pthread_join(sendThread, NULL);
    printf("sendThread joined\n");
    pthread_join(recvACKThread, NULL);
    printf("recvACKThread joined\n");

    printf("MAIN END\n");

    free(array);
    pthread_mutex_destroy(&lock);

    return 0;
}