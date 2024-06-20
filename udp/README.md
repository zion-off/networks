# UDP Client and Server

## Project Overview

The goal of this assignment is to implement a UDP client and server using socket programming in C. This project demonstrates hands-on experience with the UDP transport protocol and socket communication concepts, and it involves creating two C programs that can exchange messages using UDP: a server that listens on a specific port and a client that sends messages to the server and receives responses.

## Features

### UDP Server
- Opens a UDP socket (`SOCK_DGRAM`).
- Listens on port 9000 for incoming messages.
- Responds to received messages with the current system time and date.
- Prints a message for every incoming message, including the sender's IP address and port number.
- Continues to handle incoming requests without terminating.

### UDP Client
- Opens a UDP socket.
- Sends a message to the server on port 9000.
- Waits for and prints the reply from the server, formatted as the current time and date.
- Terminates after printing the reply.

## Requirements

- C programming language
- POSIX-compliant system (Linux, macOS)
- GCC compiler

## Installation

1. Clone the repository:
    ```bash
    git clone https://github.com/zion-off/networks.git
    cd udp
    ```

2. Compile the server program:
    ```bash
    gcc udp_server.c -o udp_server
    ```

3. Compile the client program:
    ```bash
    gcc udp_client.c -o udp_client
    ```

## Usage

### Running the Server

1. Open a terminal and navigate to the project directory.
2. Start the UDP server:
    ```bash
    ./udp_server
    ```
3. The server will start listening on port 9000 and print messages for each received request.

### Running the Client

1. Open another terminal and navigate to the project directory.
2. Start the UDP client:
    ```bash
    ./udp_client
    ```
3. The client will send a message to the server and print the server's reply (current time and date).

## Example Outputs

### Server Output

Example program output of the UDP server when it receives a message:
```
Received message from 127.0.0.1:56789
```

### Client Output

Example program output of the UDP client after receiving the server's response:
```
Server response: Wed Mar 23 18:14:21 2022
```

## Implementation Details

### UDP Server (`udp_server.c`)

1. **Socket Creation**: Create a UDP socket using `socket(AF_INET, SOCK_DGRAM, 0)`.
2. **Binding**: Bind the socket to port 9000 using `bind()`.
3. **Message Handling**: Use `recvfrom()` to receive messages and `sendto()` to send the current system time and date as a response.
4. **Logging**: Print the sender's IP address and port number for each received message.

Example code snippet for receiving messages:
```c
int sockfd;
char buffer[1024];
struct sockaddr_in servaddr, cliaddr;
socklen_t len = sizeof(cliaddr);

// Creating socket file descriptor
sockfd = socket(AF_INET, SOCK_DGRAM, 0);

// Bind the socket with the server address
bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));

while (1) {
    int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&cliaddr, &len);
    buffer[n] = '\0';
    printf("Received message from %s:%d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

    // Get current time and date
    time_t now = time(NULL);
    char *time_str = ctime(&now);

    // Send current time and date to client
    sendto(sockfd, time_str, strlen(time_str), 0, (struct sockaddr *)&cliaddr, len);
}
```

### UDP Client (`udp_client.c`)

1. **Socket Creation**: Create a UDP socket using `socket(AF_INET, SOCK_DGRAM, 0)`.
2. **Message Sending**: Send a "Time request" message to the server using `sendto()`.
3. **Response Handling**: Use `recvfrom()` to receive the server's response and print it in a meaningful format.

Example code snippet for sending and receiving messages:
```c
int sockfd;
char buffer[1024];
struct sockaddr_in servaddr;

// Creating socket file descriptor
sockfd = socket(AF_INET, SOCK_DGRAM, 0);

// Server address
servaddr.sin_family = AF_INET;
servaddr.sin_port = htons(9000);
servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

// Send message to server
char *message = "Time request";
sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

// Receive server response
socklen_t len = sizeof(servaddr);
int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&servaddr, &len);
buffer[n] = '\0';

// Print server response
printf("Server response: %s\n", buffer);
```
