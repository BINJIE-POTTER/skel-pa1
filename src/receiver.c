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

#include <math.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024
#define PACKET_DATA_SIZE (BUFFER_SIZE - sizeof(unsigned int))

typedef struct {
    unsigned int index;
    char data[BUFFER_SIZE];
} Packet;

typedef struct {
    unsigned int index; // -1 means heading package
    size_t packetNum; // Total number of packets
} InfoPacket;

bool *array;
size_t ARRAY_SIZE;

void rrecv(unsigned short int myUDPport, char* destinationFile, unsigned long long int writeRate) {
    
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    FILE *fp;
    struct timespec sleepDuration = {0, 0};

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    int bufferSize = 2 * 1024 * 1024;
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

    InfoPacket infoPacket;
    socklen_t len = sizeof(cliaddr);
    ssize_t n;
    size_t packetsLength = 0;
    while (1) {

        n = recvfrom(sockfd, &infoPacket, sizeof(infoPacket), 0, (struct sockaddr *) &cliaddr, &len);
        if (n == -1) {
            perror("recvfrom failed");
            break;
        }

        if (infoPacket.index == -1) {

            packetsLength = infoPacket.packetNum;

            ARRAY_SIZE = packetsLength;
            array = malloc(ARRAY_SIZE * sizeof(bool));
            if (array == NULL) {
                fprintf(stderr, "Failed to allocate memory for the array.\n");
                return;
            }
            for (int i = 0; i < ARRAY_SIZE; ++i) {
                array[i] = false;
            }

            break;

        }

    }

    printf("Total Packets: %zu\n", packetsLength);
    printf("Packets going to receive: %zu\n", ARRAY_SIZE);

    unsigned long long int receivedBytes = 0; // Track received bytes
    size_t packetsReceived = 0;
    Packet packet;
    while (packetsReceived != packetsLength) {

        n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *) &cliaddr, &len);
        if (n == -1) {
            perror("recvfrom failed");
            break;
        }

        if (packet.index != -1 && !array[packet.index]) {

            packetsReceived++;
            receivedBytes += n - sizeof(packet.index);
            array[packet.index] = true;

            off_t position = (off_t)packet.index * PACKET_DATA_SIZE;
            if (fseek(fp, position, SEEK_SET) != 0) {
                perror("Failed to seek in file");
                break;
            }

            if (fwrite(packet.data, 1, n - sizeof(packet.index), fp) != n - sizeof(packet.index)) {
                perror("Failed to write to file");
                break;
            }
            fflush(fp);

            unsigned int ack = packet.index;
            if (sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&cliaddr, len) < 0) {
                perror("sendto failed");
                break;
            }

            if (writeRate > 0) {
                nanosleep(&sleepDuration, NULL);
            }

        }

    }

    unsigned int ack = -1;
    for (int i = 0; i < 20; i++) {

        if (sendto(sockfd, &ack, sizeof(ack), 0, (const struct sockaddr *)&cliaddr, len) < 0) {
            perror("Failed to send file");
            break;
        } 

    }

    printf("Total received bytes: %llu\n", receivedBytes);
    printf("Packets received: %zu\n", packetsReceived);

    free(array);
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