#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <assert.h>

#include "common.h"
#include "packet.h"


/*
 * You are required to change the implementation to support
 * window size greater than one.
 * In the current implementation the window size is one, hence we have
 * only one send and receive packet
 */
tcp_packet *rcvpkt;
tcp_packet *sndpkt;

int main(int argc, char **argv) {
    int socketfd; /* socket */
    int portno; /* port to listen on */
    int clntlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    int optval; /* flag value for setsockopt */
    FILE *fp;
    char buffer[MSS_SIZE];
    struct timeval tp;
    int ACKno;

    /* 
     * check command line arguments 
     */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> FILE_RECVD\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    fp  = fopen(argv[2], "w");
    if (fp == NULL) {
        error(argv[2]);
    }

    /* 
     * socket: create the parent socket 
     */
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0) 
        error("ERROR while opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
    optval = 1;
    setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, 
            (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 
     * bind: associate the parent socket with a port 
     */
    if (bind(socketfd, (struct sockaddr *) &serveraddr, 
                sizeof(serveraddr)) < 0) 
        error("Error binding");

    /* 
     * main loop: wait for a datagram, then echo it
     */
    VLOG(DEBUG, "EPOCH time, bytes received, sequence number");

    clntlen = sizeof(clientaddr);
    while (1) {
        /*
         * recvfrom: receive a UDP datagram from a client
         */
        //VLOG(DEBUG, "waiting from server \n");
        if (recvfrom(socketfd, buffer, MSS_SIZE, 0,
                (struct sockaddr *) &clientaddr, (socklen_t *)&clntlen) < 0) {
            error("ERROR while recvfrom");
        }

        if (ftell(fp) == ((tcp_packet *) buffer)->hdr.seqno) { // received in-order packet
            rcvpkt = (tcp_packet *) buffer;
            assert(get_data_size(rcvpkt) <= DATA_SIZE);
            if ( rcvpkt->hdr.data_size == 0) {
                VLOG(INFO, "EOF has been reached");
                fclose(fp);
                break;
            }
            /* 
            * sendto: ACK back to the client 
            */
            gettimeofday(&tp, NULL);
            VLOG(DEBUG, "%lu, %d, %d", tp.tv_sec, rcvpkt->hdr.data_size, rcvpkt->hdr.seqno);
            ACKno = rcvpkt->hdr.seqno + rcvpkt->hdr.data_size;

            printf("Received Seqno : %u\n", rcvpkt->hdr.seqno);

            fseek(fp, rcvpkt->hdr.seqno, SEEK_SET);
            fwrite(rcvpkt->data, 1, rcvpkt->hdr.data_size, fp);            
        }

        sndpkt = make_packet(0);
        sndpkt->hdr.ackno = ACKno;
        printf("Sending AckNo - %u\n", sndpkt->hdr.ackno);
        sndpkt->hdr.ctr_flags = ACK;
        if (sendto(socketfd, sndpkt, TCP_HDR_SIZE, 0, 
                (struct sockaddr *) &clientaddr, clntlen) < 0) {
            error("ERROR in sendto");
        }
    }
    sndpkt = make_packet(0);
    sndpkt->hdr.ackno = 0;
    printf("Sending AckNo - %u\n", sndpkt->hdr.ackno);
    sndpkt->hdr.ctr_flags = ACK;
    if (sendto(socketfd, sndpkt, TCP_HDR_SIZE, 0, 
            (struct sockaddr *) &clientaddr, clntlen) < 0) {
        error("ERROR while sendto");
    }

    free(sndpkt);
    return 0;
}
