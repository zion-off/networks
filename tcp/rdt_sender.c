

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>

#include "packet.h"
#include "common.h"

#define STDIN_FD 0
#define SENDER_BUFFER_SIZE 10000
#define RTO_UPPER_BOUND 240000

bool end_of_file = false;

int backoff_factor = 1;
// cwnd and RETRY used to be constants, but for task 2 they are variables that are changed according to congestion
int cwnd = 1;
int fraction_counter = 0;
// RETRY is the timeout value / RTO, initialized to 3 seconds
int RETRY = 3000;
// send_base and next_seq are sequence numbers
// but out packets are buffered in an array
int next_seqno = 0;
int send_base = 0;
// so we use these integers iterate over the buffer
int next_seqno_index = 0;
int send_base_index = 0;
// used to calculate the sequence number of the packet being created
int create_seqno = 0;
// used to keep track of the sender's buffer array index
int buffer_index = 0;
int last_ackno = 0;
int duplicate_ack_count = 0;
int sockfd, serverlen;

float estimated_rtt = 10;
float dev_rtt = 10;
// initializing RTT to 0 seconds
float sample_rtt = 0;
// slow start threshold
int ssthresh = 64;

struct sockaddr_in serveraddr;

struct itimerval timer;

// structs for measuring time
// begin_time is attached to packet header
// RTT is measured using end_time when ACK is received
struct timeval begin_time, end_time;

tcp_packet *sndpkt;
tcp_packet *recvpkt;
sigset_t sigmask;

FILE *fp;
FILE *csv;

char buffer[DATA_SIZE];
tcp_packet *sender_window[SENDER_BUFFER_SIZE];

void start_timer()
{
    sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
    setitimer(ITIMER_REAL, &timer, NULL);
}

void stop_timer()
{
    sigprocmask(SIG_BLOCK, &sigmask, NULL);
}

void init_timer(int delay, void (*sig_handler)(int))
{
    signal(SIGALRM, sig_handler);

    timer.it_interval.tv_sec = delay / 1000;
    timer.it_interval.tv_usec = (delay % 1000) * 1000;
    timer.it_value.tv_sec = delay / 1000;
    timer.it_value.tv_usec = (delay % 1000) * 1000;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGALRM);
}

void update_timer(tcp_packet *pkt)
{
    // measuring RTT and caclulating RTO (RETRY) as a multiple of the estimated RTT
    gettimeofday(&end_time, NULL);
    sample_rtt = (float)((end_time.tv_sec * 1000 + (end_time.tv_usec / 1000)) - pkt->hdr.time_stamp);

    estimated_rtt = ((1.0 - 0.125) * estimated_rtt) + (0.125 * sample_rtt);
    dev_rtt = ((1.0 - 0.25) * dev_rtt) + (0.25 * fabs(sample_rtt - estimated_rtt));

    // if RTO is less than the upper bound, keep doubling it until it reaches the upper bound
    if (RETRY <= RTO_UPPER_BOUND)
    {
        RETRY = backoff_factor * (float)(estimated_rtt + (4 * dev_rtt));
    }
}

// this function is automatically called when a timeout occurs
void resend_packets(int index)
{
    printf("Buffer index: %d\n", index);
    printf("Sequence number at that index: %d\n", sender_window[index]->hdr.seqno);
    gettimeofday(&begin_time, NULL);
    sender_window[index]->hdr.time_stamp = (begin_time.tv_sec * 1000 + (begin_time.tv_usec / 1000));

    if (sendto(sockfd, sender_window[index], TCP_HDR_SIZE + get_data_size(sender_window[index]), 0,
               (const struct sockaddr *)&serveraddr, serverlen) < 0)
    {
        error("sendto");
    };
}

int max(int a, int b)
{
    return a > b ? a : b;
}

// when a timeout occurs, this signal handler is called, which allows us to see if packets are being resent because of a timeout or fast retransmit
void signal_handler(int sig)
{
    if (sig == SIGALRM)
    {
        // set ssthresh to half of cwnd
        ssthresh = max(floor((float)cwnd / 2), 2);
        // reset cwnd to 1
        cwnd = 1;
        VLOG(DEBUG, "cwnd: %d, ssthresh: %d",
             cwnd, ssthresh);
        gettimeofday(&begin_time, NULL);
        fprintf(csv, "%ld, %i\n", (begin_time.tv_sec * 1000 + (begin_time.tv_usec / 1000)), cwnd);
        VLOG(INFO, "Timeout occured. Resending packet with sequence #%d", send_base);
        buffer_index = 0;
        int expected_seqno = 0;
        for (int i = 0; i < SENDER_BUFFER_SIZE; i++)
        {
            if (expected_seqno == send_base)
            {
                buffer_index = i;
                break;
            }
            expected_seqno += get_data_size(sender_window[i]);
        }
        // print buffer_index value
        printf("Calling resend_packets with buffer index: %d\n", buffer_index);
        resend_packets(buffer_index);
    }
}

int main(int argc, char **argv)
{
    // variable declarations
    int portno, len;
    char *hostname;

    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <hostname> <port> <FILE>\n", argv[0]);
        exit(0);
    }

    hostname = argv[1];
    portno = atoi(argv[2]);
    fp = fopen(argv[3], "r");
    if (fp == NULL)
    {
        error(argv[3]);
    }

    // csv file for plotting congestion window
    csv = fopen("CWND.csv", "w");

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    bzero((char *)&serveraddr, sizeof(serveraddr));
    serverlen = sizeof(serveraddr);

    if (inet_aton(hostname, &serveraddr.sin_addr) == 0)
    {
        fprintf(stderr, "ERROR, invalid host %s\n", hostname);
        exit(0);
    }

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(portno);

    assert(MSS_SIZE - TCP_HDR_SIZE > 0);

    init_timer(RETRY, signal_handler);

    // packet generation, sending, and receiving ACKs used to be implemented with a while (!end_of_file) loop
    // but this creates an issue with packets lost AFTER the file has been read completely
    // the loop breaks, and the program can't go back and read those packets from the file again
    // so we read all of the file into a buffer, and send packets from that buffer
    for (int i = 0; i < SENDER_BUFFER_SIZE; i++)
    {
        if (i == SENDER_BUFFER_SIZE - 1)
        {
            VLOG(INFO, "File is bigger than the sender buffer. Please increase the buffer size and rerun the program.");
            return 0;
        }

        len = fread(buffer, 1, DATA_SIZE, fp);

        if (len <= 0)
        {
            sndpkt = make_packet(MSS_SIZE);
            sndpkt->hdr.seqno = -1;
            sndpkt->hdr.data_size = 0;
            sender_window[i] = (tcp_packet *)sndpkt;
            break;
        }

        // packet generation, buffering
        // (but NOT timestamping at the moment)
        // (we'll timestamp when we're actually sending the packet)
        sndpkt = make_packet(len);
        memcpy(sndpkt->data, buffer, len);
        sndpkt->hdr.seqno = create_seqno;
        create_seqno += len;
        sender_window[i] = (tcp_packet *)sndpkt;
        // printf("Created packet with sequence number %d at buffer index: %d \n", sndpkt->hdr.seqno, i);
    }

    // sending the first cwnd packets
    for (int i = 0; i < cwnd; i++)
    {
        len = sender_window[i]->hdr.data_size;
        next_seqno += len;

        // timestamping the packet during sending
        gettimeofday(&begin_time, NULL);
        sender_window[i]->hdr.time_stamp = (begin_time.tv_sec * 1000 + (begin_time.tv_usec / 1000));

        VLOG(DEBUG, "cwnd: %d, ssthresh: %d",
             cwnd, ssthresh);
        VLOG(DEBUG, "Sending packet %d to %s",
             sender_window[i]->hdr.seqno, inet_ntoa(serveraddr.sin_addr));

        if (sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
                   (const struct sockaddr *)&serveraddr, serverlen) < 0)
        {
            error("sendto");
        }
    }

    while (true)
    {
        start_timer();

        if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
                     (struct sockaddr *)&serveraddr, (socklen_t *)&serverlen) < 0)
        {
            error("recvfrom");
        }
        recvpkt = (tcp_packet *)buffer;
        printf("Received ACK with sequence #%d \n", recvpkt->hdr.ackno);
        assert(get_data_size(recvpkt) <= DATA_SIZE);

        update_timer(recvpkt);

        // case 0: file transfer complete
        if (recvpkt->hdr.ackno == -1)
        {
            VLOG(INFO, "File transfer complete.");
            break;
        }

        // case 1: duplicate ACK received
        if (recvpkt->hdr.ackno == last_ackno)
        {
            // if a duplicate ACK is received, increment the counter
            VLOG(INFO, "Duplicate ACK received");
            duplicate_ack_count++;
            // if 3 duplicate ACKs have been received, do fast retransmit
            if (duplicate_ack_count >= 3)
            {
                duplicate_ack_count = 0;
                buffer_index = 0;
                int expected_seqno = 0;
                for (int i = 0; i < SENDER_BUFFER_SIZE; i++)
                {
                    if (expected_seqno == send_base)
                    {
                        buffer_index = i;
                        break;
                    }
                    expected_seqno += get_data_size(sender_window[i]);
                }
                printf("Buffer index: %d\n", buffer_index);
                printf("Sequence number at that index: %d\n", sender_window[buffer_index]->hdr.seqno);
                VLOG(INFO, "3 duplicate ACKs received. Fast retransmitting packet %d", recvpkt->hdr.ackno);
                resend_packets(buffer_index);
                // adjust
                ssthresh = max(floor((float)cwnd / 2), 2);
                // reset cwnd to 1
                cwnd = 1;
                VLOG(DEBUG, "cwnd: %d, ssthresh: %d",
                     cwnd, ssthresh);
                gettimeofday(&begin_time, NULL);
                fprintf(csv, "%ld, %i\n", (begin_time.tv_sec * 1000) + (begin_time.tv_usec / 1000), cwnd);
            }
        }
        // case 2: new ACK received
        else
        {
            // reset the duplicate ACK counter
            last_ackno = recvpkt->hdr.ackno;
            duplicate_ack_count = 0;
            // push the window forward
            send_base = recvpkt->hdr.ackno;

            // case 1: still in slow start, increase cwnd by 1
            if (cwnd <= ssthresh)
            {
                cwnd++;
            }
            // case 2: congestion avoidance, cwnd += 1/cwnd
            else if (cwnd >= ssthresh)
            {
                fraction_counter++;
                if (fraction_counter == (cwnd + 1))
                {
                    cwnd++;
                    fraction_counter = 0;
                }
            }
            VLOG(DEBUG, "cwnd: %d, ssthresh: %d",
                 cwnd, ssthresh);
            gettimeofday(&begin_time, NULL);
            fprintf(csv, "%ld, %i\n", (begin_time.tv_sec * 1000) + (begin_time.tv_usec / 1000), cwnd);
        }

        if (next_seqno <= recvpkt->hdr.ackno)
        {
            for (int i=0; i<SENDER_BUFFER_SIZE; i++) {
                if (sender_window[i]->hdr.seqno == send_base) {
                    send_base_index = i;
                    break;
                }
            }

            int sub_cwnd = cwnd - (next_seqno_index - send_base_index);

            for (int i = 0; i < sub_cwnd; i++)
            {
                for (int i=0; i<SENDER_BUFFER_SIZE; i++) {
                if (sender_window[i]->hdr.seqno == next_seqno) {
                    next_seqno_index = i;
                    break;
                }
            }

                if (sender_window[next_seqno_index]->hdr.seqno == -1)
                {
                    end_of_file = true;
                    break;
                }

                VLOG(DEBUG, "Sending packet %d to %s",
                     send_base, inet_ntoa(serveraddr.sin_addr));

                gettimeofday(&begin_time, NULL);
                sender_window[next_seqno_index]->hdr.time_stamp = (begin_time.tv_sec * 1000 + (begin_time.tv_usec / 1000));

                if (sendto(sockfd, sender_window[next_seqno_index], TCP_HDR_SIZE + get_data_size(sender_window[next_seqno_index]), 0,
                           (const struct sockaddr *)&serveraddr, serverlen) < 0)
                {
                    error("sendto");
                }
                next_seqno += DATA_SIZE;
            }
        }

        stop_timer();
    }
    free(sndpkt);
    fclose(fp);
    fclose(csv);
    return 0;
}
