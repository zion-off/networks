#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>

#define PORT 9000

int main()
{
    // create a socket()
    /*
    network_socket: stores the file descriptor for the socket

    AF_INET: specifies address family IPv4 (tells server that
    we're creating a socket that will communicate using the Internet Protocol version 4.

    SOCK_DGRAM: specifies type of socket (Datagram socket UDP)

    0: specifies protocol. 0 means the system will choose the appropriate
    protocol based on the address family & socket type. Since we have
    AF_INET and SOCK_DGRAM here, this means using UDP.
    */
    int network_socket = socket(AF_INET, SOCK_DGRAM, 0);

    // check for fail error
    if (network_socket < 0)
    {
        printf("socket creation failed..\n");
        exit(EXIT_FAILURE);
    }

    // optional call to set socket options
    // REUSEADDR to avoid errors where bind() wouldn't be allowed in case the address is already used
    if (setsockopt(network_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        perror("setsockopt error");
        exit(-1);
    }

    // specify an address for the socket we want to connect to

    /*
    htons: converts port number from host byte order to network byte order

    INADDR_ANY: specical constant meaning that the socket will bind to
    all available network interfaces on the machine, so basically tells
    the socket to listen on all available network interfaces and
    IP addresses of the server
    */
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;   // address family
    server_address.sin_port = htons(PORT); // set port
    server_address.sin_addr.s_addr = INADDR_ANY;

    struct sockaddr_in client_address;
    socklen_t client_address_len = sizeof(client_address);

    // bind the socket to our specified IP and port
    if (bind(network_socket,
             (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0)
    {
        printf("socket bind failed..\n");
        exit(EXIT_FAILURE);
    }

    // print welcome message
    printf("Time server started:\n");

    while (1)
    {
        char buffer[256];

        // receive data from the socket and store it in buffer
        /*
        4th param: additional flags, usually set to 0

        5th param: for source address of the sender. 0 means we're not
        interested in knowing the source address of the incoming data/
        If we want to retrieve information about the sender's address,
        we should provide a pointer to a struct sockaddr_in here and the
        function will fill in that structure with the sender's address
        information.

        6th param: for size of the source address structure if set in
        5th param. Here it's 0 because we aren't interested in the
        sender's address.
        */
        recvfrom(network_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_address, &client_address_len);

        // INET_ADDRSTRLEN is the maximum length of a string that can hold
        char client_ip[INET_ADDRSTRLEN];
        // convert IPv4 and IPv6 addresses from binary to text form
        inet_ntop(AF_INET, &(client_address.sin_addr), client_ip, INET_ADDRSTRLEN);
        // convert port number from network byte order to host byte order
        int client_port = ntohs(client_address.sin_port);
        // print client IP and port
        printf("Time request from %s:%d\n", client_ip, client_port);

        // create time_t variable
        time_t current_time;
        // struct to hold time and date
        struct tm *time_info;
        // get current time
        time(&current_time);
        // convert current time to local time
        time_info = localtime(&current_time);

        // create a timestamp
        char timestamp[64];
        // format the timestamp
        strftime(timestamp, sizeof(timestamp), "Server time: %a %b %e %H:%M:%S %Y", time_info);

        // reply to the client
        ssize_t bytes_sent = sendto(network_socket, timestamp, strlen(timestamp), 0, (struct sockaddr *)&client_address, sizeof(client_address));
        if (bytes_sent < 0)
        {
            perror("sendto");
            exit(EXIT_FAILURE);
        }

        /*
        recvfrom is blocking, returns after data is received from socket
        - If there is data available on the socket to be received, recvfrom
        will immediately return with that data
        - If there is no data available at the moment of the recvfrom call,
        the function will pause/block the program until the data srrives on
        the socket. It will not proceed to next line of code until data is
        received or an error occurs.
        - Once data is receieved, the recvfrom returns control to program,
        and we can continue executing the next code lines.

        recvfrom also returns the total number of bytes received.
        */

        if (strcmp(buffer, "exit") == 0)
        {
            printf("Server Exit...\n");
            break;
        }
    }

    // close the socket to close the connection & free up port used by socket
    close(network_socket);

    return 0;
}
