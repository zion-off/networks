#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>

#define PORT 9000

int main()
{
    // socket()
    int network_socket = socket(AF_INET, SOCK_DGRAM, 0);

    // check for fail error
    if (network_socket == -1)
    {
        printf("socket creation failed..\n");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(network_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        perror("setsockopt error");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    // server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    // set socket to non-blocking
    // allow the program to continue even if the socket on the other end is not ready to send/receive data
    // the use of non-blocking sockets is explained later in the code
    fcntl(network_socket, F_SETFL, O_NONBLOCK);

    while (1)
    {
        char buffer[256] = "Time request";

        /*
        5th param: pointer to struct sockaddr_in that specifies the
        destination address and port to which the data should be sent,
        which is typically the address of the server or recepient of the data

        6th param: size of the server address structure
        */
        sendto(network_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&server_address, sizeof(server_address));

        /*sendto is blocking: returns after data is sent from socket
        After data is sent over the socket, the program is paused/blocked
        at the sendto function call. It won't proceed to the next line of
        code until the data has been sent or an error orccurs.
        Once the data is successfuly sent, block removed and sentto returns
        control to program so we can continue executing the next code lines.

        sendto also returns the total number of bytes sent.
        */

        /*explanation for using non blocking sockets and putting the program to sleep for 2 seconds:
         since we are using UDP, there is no connection being established -- the client only sends datagrams. this means that if the client starts sending requests before the server is live, the recvfrom functio may be blocking the client's execution until it receives something. even when the server is ready, the client can't go on, because it will still be waiting for a response to the first request it sent, which was before the server was even running. this isssue is resolved by using non blocking sockets, so the program can execute regardless.
         also, the client here terminates after printing response only once, but it may already have sent a burst of requests to the server, which makes the server print superfluous request details. so we wait a second between sending each request.*/
        // sleep for 1 second
        sleep(1);

        // receive response from server
        char server_response[256];
        // receive data from the socket and store it in the buffer
        recvfrom(network_socket, server_response, sizeof(server_response), 0, NULL, NULL);
        // print server's response
        if (server_response[0] != '\0')
        {
            printf("%s\n", server_response);
            break;
        }
    }
    close(network_socket);
}