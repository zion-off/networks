#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define PORT 21

int port;
int network_socket;
char server_ip[256];
char auth_state[256];
char response[1023];
char buffer[1023];
char *command;
char client_name[1024] = " ";
size_t buffer_size = 256;

struct sockaddr_in client;
socklen_t client_size = sizeof(client);

// print error message and exit
void handleError(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

// parse and extract an IP address and port number
void Port(char *buffer)
{
    char *ip[256];
    int port_number;
    int count;
    char *c;
    char *c_array[6];
    char buffer_copy[256];
    strcpy(buffer_copy, buffer);

    c = strtok(buffer_copy, " -.,");
    while (c != NULL && count < 6)
    {
        c_array[count++] = c;
        c = strtok(NULL, " -.,");
    }

    if (count >= 5)
    {
        snprintf(ip, sizeof(ip), "%s.%s.%s.%s", c_array[0], c_array[1], c_array[2], c_array[3]);
        port_number = atoi(c_array[4]) * 256 + atoi(c_array[5]);
        unsigned long port_number_ulong = (unsigned long)port_number;
        printf("%s %lu\n", *ip, port_number_ulong);
    }
    else
    {
        printf("Invalid PORT command\n");
    }
}

// creates a socket, and connects it to a server using a specified port and address
void establish_connection()
{
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    network_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (network_socket == -1)
    {
        handleError("Socket creation failed.");
    }

    int connection_status = connect(network_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    if (connection_status == -1)
    {
        handleError("Error making a connection to the remote socket.");
    }
    printf("Connected to server.\n");
}

int main()
{
    memset(&auth_state, 0, sizeof(auth_state));

    establish_connection();

    getsockname(network_socket, (struct sockaddr *)&client, &client_size);
    command = (char *)malloc(buffer_size * sizeof(char));
    port = ntohs(client.sin_port);
    sprintf(server_ip, "%s", inet_ntoa(client.sin_addr));

    // main loop to handle client commands
    while (1)
    {
        // clear buffers
        memset(&command, 0, sizeof(command));
        memset(&buffer, 0, sizeof(buffer));
        memset(&response, 0, sizeof(response));

        printf("ftp> ");

        getline(&command, &buffer_size, stdin);

        if (strcmp(command, "\n") == 0)
        {
            printf("\n");
            continue;
        }

        char *first_part = strtok(command, " \n");
        char *second_part = strtok(NULL, " \n");

        if (strcmp(first_part, "PORT") == 0 && (strcmp(auth_state, "loggedIn") == 0))
        {
            Port(second_part);
        }

        // send to server, receive response, then store the username in client_name
        else if (strcmp(first_part, "USER") == 0)
        {
            strcpy(buffer, "USER ");
            strcat(buffer, second_part);
            send(network_socket, buffer, strlen(buffer), 0);
            recv(network_socket, buffer, sizeof(buffer), 0);
            printf("%s\n", buffer);
            memset(client_name, 0, sizeof(client_name));
            strcpy(client_name, second_part);
        }

        // send to server, receive response, then store the password in client_name
        else if (strcmp(first_part, "PASS") == 0)
        {
            strcpy(buffer, "PASS ");
            strcat(buffer, second_part);
            send(network_socket, buffer, strlen(buffer), 0);
            recv(network_socket, buffer, sizeof(buffer), 0);
            char *first_part = strtok(buffer, " ");
            char *second_part = strtok(NULL, "\n");
            strcpy(auth_state, first_part);
            printf("%s\n", second_part);
        }

        // code to upload to server
        else if (strcmp(first_part, "STOR") == 0 && (strcmp(auth_state, "loggedIn") == 0))
        {
            // create a new socket to listen on
            char str[256];
            // use a different port for subsequent transfers
            port++;
            snprintf(buffer, sizeof(buffer), "STOR %s 127.0.0.1:%lu %s", second_part, port, client_name);
            send(network_socket, buffer, sizeof(buffer), 0);
            recv(network_socket, response, sizeof(response), 0);
            printf("%s\n", response);

            int new_socket_desc, new_client_sock, new_client_size;
            struct sockaddr_in new_server_addr, new_client_addr;

            new_socket_desc = socket(AF_INET, SOCK_STREAM, 0);
            if (new_socket_desc < 0)
            {
                printf("Error while creating socket\n");
                continue;
            }

            int value = 1;
            setsockopt(new_socket_desc, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
            new_server_addr.sin_family = AF_INET;
            new_server_addr.sin_port = htons(port);
            new_server_addr.sin_addr.s_addr = inet_addr(server_ip);

            if (bind(new_socket_desc, (struct sockaddr *)&new_server_addr, sizeof(new_server_addr)) < 0)
            {
                printf("Couldn't bind to the port\n");
                continue;
            }
            else
            {
                printf("Binded Correctly\n");
            }

            if (listen(new_socket_desc, 5) < 0)
            {
                printf("Error while listening\n");
                continue;
            }

            new_client_size = sizeof(new_client_addr);
            new_client_sock = accept(new_socket_desc, (struct sockaddr *)&new_client_addr, &new_client_size);

            if (new_client_sock < 0)
            {
                printf("Can't accept\n");
                continue;
            }

            // open the file to be uploaded
            FILE *file;
            char *file_buffer;
            unsigned long file_length;

            file = fopen(second_part, "rb");

            // send flag to server to indicate file doesn't exist
            if (file == NULL)
            {
                int file_flag = 0;
                send(new_client_sock, &file_flag, sizeof(int), 0);
                printf("550 No such file or directory.");
                continue;
            }

            else
            {
                // flag to indicate to server that file is sent
                int file_flag = 1;
                send(new_client_sock, &file_flag, sizeof(int), 0);

                struct stat st;
                stat(file, &st);
                unsigned long size = st.st_size;

                fseek(file, 0, SEEK_END);
                file_length = ftell(file);
                fseek(file, 0, SEEK_SET);

                file_buffer = (char *)malloc(file_length + 1);
                if (!file_buffer)
                {
                    fprintf(stderr, "Dynamic memory couldn't be allocated.\n");
                    fclose(file);
                    continue;
                }

                // read file into buffer
                fread(file_buffer, file_length, 1, file);
                // send file & its size to server
                send(new_client_sock, &file_length, sizeof(long), 0);
                send(new_client_sock, file_buffer, file_length + 1, 0);
                fclose(file);

                // receive ACK from server
                char received_file[256];
                recv(new_client_sock, received_file, sizeof(received_file), 0);
                printf("%s\n", received_file);

                free(file_buffer);
            }

            close(new_client_sock);
            close(new_socket_desc);
        }

        else if (strcmp(first_part, "RETR") == 0 && (strcmp(auth_state, "loggedIn") == 0))
        {
            // use a different port for subsequent transfers
            port++;
            snprintf(buffer, sizeof(buffer), "RETR %s 127.0.0.1:%lu %s", second_part, port, client_name);
            // send request to server, receive ACK
            send(network_socket, buffer, sizeof(buffer), 0);
            recv(network_socket, response, sizeof(response), 0);
            printf("%s\n", response);
            int new_socket_desc, new_client_sock, new_client_size;
            struct sockaddr_in new_server_addr, new_client_addr;
            new_socket_desc = socket(AF_INET, SOCK_STREAM, 0);

            if (new_socket_desc < 0)
            {
                printf("Error while creating socket\n");
                continue;
            }

            int value = 1;
            setsockopt(new_socket_desc, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

            new_server_addr.sin_family = AF_INET;
            new_server_addr.sin_port = htons(port);
            new_server_addr.sin_addr.s_addr = inet_addr(server_ip);

            if (bind(new_socket_desc, (struct sockaddr *)&new_server_addr, sizeof(new_server_addr)) < 0)
            {
                printf("Couldn't bind to the port\n");
                continue;
            }
            else
                printf("Binded Correctly\n");

            if (listen(new_socket_desc, 5) < 0)
            {
                printf("Error while listening\n");
                continue;
            }

            new_client_size = sizeof(new_client_addr);
            new_client_sock = accept(new_socket_desc, (struct sockaddr *)&new_client_addr, &new_client_size);

            if (new_client_sock < 0)
            {
                printf("Can't accept\n");
                return -1;
            }

            int file_flag;
            recv(new_client_sock, &file_flag, sizeof(int), 0);

            if (file_flag)
            {
                // declare a variable to hold the size of the file
                long size;
                // receive the size of the file from the server
                recv(new_client_sock, &size, sizeof(long), 0);

                char *file_data = malloc(size);
                // receive the file data from the client
                recv(new_client_sock, file_data, size, 0);

                FILE *write_pointer = fopen(second_part, "wb");
                if (write_pointer == NULL)
                {
                    handleError("Error opening file");
                }

                // write the received file data to the file
                fwrite(file_data, size, 1, write_pointer);

                char received_file[100] = {0};
                int received_bytes = recv(new_client_sock, received_file, sizeof(received_file), 0);
                if (received_bytes)
                {
                    printf("200 PORT command successful.\n");
                }

                printf("226 Transfer complete\n");
                free(file_data);
                fclose(write_pointer);
            }
            else
            {
                printf("550 No such file or directory.\n");
            }
            close(new_client_sock);
            close(new_socket_desc);
        }

        // server side ls
        else if (strcmp(first_part, "LIST") == 0 && (strcmp(auth_state, "loggedIn") == 0))
        {
            // use a different port for subsequent transfers
            port++;
            snprintf(buffer, sizeof(buffer), "LIST 127.0.0.1:%lu %s", port, client_name);

            printf("FTP port: %lu\n", port);
            printf("IP and port %s %lu\n", server_ip, port);

            send(network_socket, buffer, sizeof(buffer), 0);

            int received_bytes = recv(network_socket, response, sizeof(response), 0);

            printf("%s\n", response);

            // create a new socket for data transfer
            int new_socket_desc, new_client_sock, new_client_size;
            struct sockaddr_in new_server_addr, new_client_addr;
            new_socket_desc = socket(AF_INET, SOCK_STREAM, 0);

            if (new_socket_desc < 0)
            {
                printf("Error while creating socket\n");
                continue;
            }

            // set socket options to reuse address
            int value = 1;
            setsockopt(new_socket_desc, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

            // set up the server address for the new socket
            new_server_addr.sin_family = AF_INET;
            new_server_addr.sin_port = htons(port);
            new_server_addr.sin_addr.s_addr = inet_addr(server_ip);

            // bind the new socket to the specified port
            if (bind(new_socket_desc, (struct sockaddr *)&new_server_addr, sizeof(new_server_addr)) < 0)
            {
                printf("Couldn't bind to the port\n");
                continue;
            }
            else
                printf("Binded Correctly\n");

            // listen for incoming connections on the new socket
            if (listen(new_socket_desc, 5) < 0)
            {
                printf("Error while listening\n");
                continue;
            }

            // accept a new client connection on the new socket
            new_client_size = sizeof(new_client_addr);
            new_client_sock = accept(new_socket_desc, (struct sockaddr *)&new_client_addr, &new_client_size);

            if (new_client_sock < 0)
            {
                printf("Can't accept\n");
                return -1;
            }

            // receive the file list size from the server
            long size;
            char *fileDat;
            recv(new_client_sock, &size, sizeof(long), 0);

            // allocate memory to store the file list
            fileDat = malloc(size);

            // receive the file list from the server
            recv(new_client_sock, fileDat, size, 0);
            printf("%s\n", fileDat);
            printf("226 Transfer complete\n");

            close(new_client_sock);
            close(new_socket_desc);
        }

        // client side ls
        else if (strcmp(first_part, "!LIST") == 0 && (strcmp(auth_state, "loggedIn") == 0))
        {
            system("ls");
        }

        // change working directory of server
        else if (strcmp(first_part, "CWD") == 0 && (strcmp(auth_state, "loggedIn") == 0))
        {

            snprintf(buffer, sizeof(buffer), "CWD %s %s", second_part, client_name);
            send(network_socket, buffer, strlen(buffer), 0);
            recv(network_socket, buffer, sizeof(buffer), 0);
            printf("%s\n", buffer);
        }

        // change working directory of client
        else if (strcmp(first_part, "!CWD") == 0)
        {
            chdir(second_part);
        }

        // print working directory of server
        else if (strcmp(first_part, "PWD") == 0 && (strcmp(auth_state, "loggedIn") == 0))
        {
            snprintf(buffer, sizeof(buffer), "%s %s %s", first_part, " ", client_name);
            send(network_socket, buffer, strlen(buffer), 0);
            recv(network_socket, buffer, sizeof(buffer), 0);
            printf("%s\n", buffer);
        }

        // print working directory of client
        else if (strcmp(first_part, "!PWD") == 0)
        {
            system("pwd");
        }

        else if (strcmp(first_part, "QUIT") == 0)
        {
            memset(response, '\0', sizeof(response));
            strcpy(buffer, "QUIT");
            printf("%s\n", buffer);
            send(network_socket, buffer, sizeof(buffer), 0);
            recv(network_socket, response, sizeof(response), 0);
            printf("%s\n", response);
            break;
        }

        else
        {
            printf("202 Command not implemented\n");
        }
    }

    close(network_socket);

    return 0;
}