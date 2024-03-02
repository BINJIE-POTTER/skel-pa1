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


// void rrecv(unsigned short int myUDPport, 
//             char* destinationFile, 
//             unsigned long long int writeRate) {

// }

// int main(int argc, char** argv) {
//     // This is a skeleton of a main function.
//     // You should implement this function more completely
//     // so that one can invoke the file transfer from the
//     // command line.

//     unsigned short int udpPort;

//     if (argc != 3) {
//         fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
//         exit(1);
//     }

//     udpPort = (unsigned short int) atoi(argv[1]);
// }

void rrecv(unsigned short int myUDPport, char* destinationFile, unsigned long long int writeRate) {

    (void)writeRate;

    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[BUFFER_SIZE];
    FILE *fp;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

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

    printf("Receiver is listening on port %hu\n", myUDPport);

    socklen_t len = sizeof(cliaddr);
    int n;
    while ((n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &cliaddr, &len)) > 0) {
        fwrite(buffer, 1, n, fp);
        printf("wrote...");
    }

    if (n < 0) {
        perror("recvfrom failed");
    }

    fclose(fp);
    close(sockfd);

    printf("Receiver terminated\n");

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