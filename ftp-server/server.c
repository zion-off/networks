// importing required libraries
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>

#define PORT 21
#define MAX_USERS 50

int num_users = 0;
unsigned long port_ftp;
char ip_addr[256];

struct user
{
        int login_stat;
        char user_name[1024]; // user name
        char user_pass[1024]; // user password
        char user_dir[1024];  // user directory
};

struct user users[MAX_USERS]; // max number of users

void read_login()
{
        FILE *fp;
        fp = fopen("login.txt", "r");
        if (fp == NULL)
        {
                printf("Error opening file!\n");
                exit(1);
        }
        int i = 0;
        while (fscanf(fp, "%s %s", users[i].user_name, users[i].user_pass) != EOF)
        {
                num_users++;
                i++;
        }
        fclose(fp);
}

int check_pass(char *buffer, int socket, char *name)
{
        // checking password
        for (int i = 0; i < num_users; i++)
        {
                if ((strcmp(users[i].user_name, name) == 0) && (strcmp(users[i].user_pass, buffer) == 0))
                {
                        printf("Name: %s has logged in\n", name);
                        send(socket, "loggedIn 230 User logged in, proceed.", 99, 0);
                        return 1;
                }
        }
        printf("not logged in\n");
        send(socket, "notLogged 530 Not Logged In.", 99, 0);
        return 0;
}

struct user check_username(char *buffer, int socket, char *name)
{
        // checking user name
        for (int i = 0; i < num_users; i++)
        {
                if (strcmp(users[i].user_name, buffer) == 0)
                { // Finding the user
                        send(socket, "331 Username OK, need password.", 99, 0);
                        users[i].login_stat = 1;
                        strcpy(name, buffer);
                        return users[i];
                }
        }
        send(socket, "530 Not logged in.", 99, 0);
}

int main()
{
        read_login(); // loading the data users into struct
        char username[256];

        strcpy(username, "xxxxxxx");

        char path[1024];
        getcwd(path, 1024);

        // setting parameters for users
        for (int i = 0; i < num_users; i++)
        {
                strcpy(users[i].user_dir, path);
                users[i].login_stat = 0;
        }

        int srvr_sock = socket(AF_INET, SOCK_STREAM, 0);
        printf("Server fd = %d \n", srvr_sock);

        // check for fail error
        if (srvr_sock < 0)
        {
                perror("socket:");
                exit(EXIT_FAILURE);
        }

        // setsock
        int value = 1;
        setsockopt(srvr_sock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

        // defining the server address structure
        struct sockaddr_in server_address;
        bzero(&server_address, sizeof(server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(PORT);
        server_address.sin_addr.s_addr = INADDR_ANY;

        // binding
        if (bind(srvr_sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
        {
                perror("bind failed");
                exit(EXIT_FAILURE);
        }

        // listening (server)
        if (listen(srvr_sock, 5) < 0)
        {
                perror("listen failed");
                close(srvr_sock);
                exit(EXIT_FAILURE);
        }

        // declaring 2 fd sets
        fd_set all_socks;
        fd_set ready_sockets;

        // initalizing set of all sockets
        FD_ZERO(&all_socks);

        // adding one socket to the fd set of all sockets
        FD_SET(srvr_sock, &all_socks);
        printf("Server is listening\n");

        while (1)
        {
                // until QUIT command
                ready_sockets = all_socks;
                if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0)
                {
                        perror("select error");
                        exit(EXIT_FAILURE);
                }
                for (int fd = 0; fd < FD_SETSIZE; fd++)
                {
                        // is fd SET
                        if (FD_ISSET(fd, &ready_sockets))
                        {
                                if (fd == srvr_sock)
                                {
                                        // accept that new connection
                                        int client_sd = accept(srvr_sock, 0, 0);
                                        printf("Client Connected fd = %d \n", client_sd);

                                        // add the newly accepted socket to the set of all sockets that we are watching
                                        FD_SET(client_sd, &all_socks);
                                }
                                else
                                {
                                        char buffer[1023]; // for storing
                                        char response[1023];

                                        bzero(response, sizeof(response));
                                        bzero(buffer, sizeof(buffer));

                                        int bytes = recv(fd, buffer, sizeof(buffer), 0);

                                        if (bytes == 0)
                                        { // closing the connection (by the client)
                                                printf("Connection closed from client side\n");
                                                // we are done, close fd
                                                close(fd);
                                                // once we are done handling the connection, remove the socket from the list of file descriptors that we are watching
                                                FD_CLR(fd, &all_socks);
                                        }
                                        else
                                        {
                                                // splitting the response
                                                char *tkn_arr[3];
                                                int index = 0;

                                                // Seperating by spaces
                                                tkn_arr[index] = strtok(buffer, " \n");
                                                while (tkn_arr[index] != NULL)
                                                {
                                                        tkn_arr[++index] = strtok(NULL, " \n");
                                                }

                                                if (strcmp(tkn_arr[0], "PWD") == 0)
                                                {
                                                        for (int i = 0; i < num_users; i++)
                                                        {
                                                                // Finding the user and getting the directory of that user
                                                                if (strcmp(users[i].user_name, tkn_arr[1]) == 0)
                                                                {
                                                                        strcpy(response, users[i].user_dir);
                                                                        send(fd, response, sizeof(response), 0);
                                                                }
                                                        }
                                                }
                                                if (strcmp(tkn_arr[0], "CWD") == 0)
                                                {
                                                        char *command = tkn_arr[1];
                                                        char cur_path[1024];
                                                        char cmd_path[1024];
                                                        for (int i = 0; i < num_users; i++)
                                                        {
                                                                if (strcmp(users[i].user_name, tkn_arr[2]) == 0)
                                                                {
                                                                        // Moving to the directory of the user
                                                                        chdir(users[i].user_dir);
                                                                }
                                                        }
                                                        if (chdir(command) == 0)
                                                        {
                                                                send(fd, "200 directory changed to pathname/foldername", sizeof(response), 0);
                                                                getcwd(cur_path, 1024);
                                                        }
                                                        else
                                                        {
                                                                perror("chdir failed");
                                                                send(fd, "550 No such file or directory", sizeof(response), 0);
                                                        }
                                                        for (int i = 0; i < num_users; i++)
                                                        {
                                                                if (strcmp(users[i].user_name, tkn_arr[2]) == 0)
                                                                {
                                                                        system("pwd>>cwdtmp.txt");
                                                                        // writing the pwd to a file
                                                                        FILE *fp;
                                                                        fp = fopen("cwdtmp.txt", "r");
                                                                        if (fp == NULL)
                                                                                exit(1);
                                                                        fgets(cmd_path, 1024, fp); // getting the pwd from the file
                                                                        fclose(fopen("cwdtmp.txt", "w"));
                                                                        system("rm cwdtmp.txt ");
                                                                        strcpy(users[i].user_dir, cmd_path);
                                                                }
                                                        }
                                                }
                                                else if (strcmp(tkn_arr[0], "PORT") == 0)
                                                {
                                                        // spliting into ip and port
                                                        char *cmd1 = strtok(tkn_arr[1], ":");
                                                        char *cmd2 = strtok(NULL, " \n");
                                                        strcpy(ip_addr, cmd1);

                                                        port_ftp = atoi(cmd2);
                                                        printf("%s %lu\n", ip_addr, port_ftp);
                                                }
                                                else if (strcmp(tkn_arr[0], "USER") == 0)
                                                {
                                                        check_username(tkn_arr[1], fd, username);
                                                }
                                                else if (strcmp(tkn_arr[0], "PASS") == 0)
                                                {
                                                        check_pass(tkn_arr[1], fd, username);
                                                }
                                                else if (strcmp(tkn_arr[0], "LIST") == 0)
                                                {
                                                        char *cmd1 = strtok(tkn_arr[1], ":");
                                                        char *cmd2 = strtok(NULL, " \n");

                                                        strcpy(ip_addr, cmd1);
                                                        port_ftp = atoi(cmd2); // converting the FTP port to integer

                                                        printf("%s %lu\n", ip_addr, port_ftp);

                                                        strcpy(response, "200 PORT command successful.");
                                                        send(fd, response, sizeof(response), 0);

                                                        // forking to excecute LIST
                                                        pid_t child1 = fork();

                                                        if (child1 < 0)
                                                        { /* error  */
                                                                fprintf(stderr, "Fork Failed");
                                                                continue;
                                                        }

                                                        else if (child1 == 0)
                                                        {
                                                                // child process
                                                                int new_socket_desc;
                                                                struct sockaddr_in new_server_addr, bind_server_addr;

                                                                // creating socket:
                                                                new_socket_desc = socket(AF_INET, SOCK_STREAM, 0);

                                                                if (new_socket_desc < 0)
                                                                {
                                                                        printf("Unable to create socket\n");
                                                                        return -1;
                                                                }

                                                                // setting port and IP the same as server-side:
                                                                new_server_addr.sin_family = AF_INET;
                                                                new_server_addr.sin_port = htons(port_ftp);
                                                                new_server_addr.sin_addr.s_addr = inet_addr(ip_addr);

                                                                // assigning port number 12010 by
                                                                // binding client with that port
                                                                bind_server_addr.sin_family = AF_INET;
                                                                bind_server_addr.sin_addr.s_addr = inet_addr(ip_addr);
                                                                bind_server_addr.sin_port = htons(2020); // unable to use PORT 20 here for FTP since it is blocked by OS

                                                                if (bind(new_socket_desc, (struct sockaddr *)&bind_server_addr, sizeof(struct sockaddr_in)) == 0)
                                                                        printf("Binded Correctly\n");
                                                                else
                                                                        printf("Unable to bind\n");
                                                                // Send connection request to server:

                                                                socklen_t addr_size = sizeof new_server_addr;
                                                                if (connect(new_socket_desc, (struct sockaddr *)&new_server_addr, addr_size) < 0)
                                                                {
                                                                        printf("Unable to connect\n");
                                                                        return -1;
                                                                }
                                                                printf("Connected with client successfully\n");
                                                                // char cur_path[1024];
                                                                char cmd_path[1024];
                                                                for (int i = 0; i < num_users; i++)
                                                                {
                                                                        if (strcmp(users[i].user_name, tkn_arr[2]) == 0)
                                                                        {
                                                                                chdir(users[i].user_dir);
                                                                        }
                                                                }

                                                                // sending LIST data to the client
                                                                system("ls>list.txt");
                                                                FILE *fp;
                                                                fp = fopen("list.txt", "r");
                                                                if (fp == NULL)
                                                                {
                                                                        printf("Error opening file!\n");
                                                                        exit(1);
                                                                }

                                                                fseek(fp, 0, SEEK_END);
                                                                long size = ftell(fp);  // get current file pointer
                                                                fseek(fp, 0, SEEK_SET); // seek back to beginning of file
                                                                                        // proceed with allocating memory and reading the file
                                                                // send the size
                                                                char *string = malloc(size + 1);
                                                                fread(string, size, 1, fp);
                                                                fclose(fp);

                                                                for (int i = 0; i < num_users; i++)
                                                                {
                                                                        system("pwd>>listTmp.txt");
                                                                        // writing the pwd to a file
                                                                        FILE *fp;
                                                                        fp = fopen("listTmp.txt", "r");
                                                                        if (fp == NULL)
                                                                        {
                                                                                exit(1);
                                                                        }
                                                                        fgets(cmd_path, 1024, fp); // getting the pwd from the file
                                                                        if (strcmp(users[i].user_name, tkn_arr[2]) == 0)
                                                                        {
                                                                                strcpy(users[i].user_dir, cmd_path);
                                                                        }
                                                                }
                                                                // send the size;

                                                                send(new_socket_desc, &size, sizeof(long), 0);

                                                                send(new_socket_desc, string, size + 1, 0);
                                                                system("rm list.txt"); // Removing the list.txt file after sending the data
                                                                system("rm listTmp.txt");

                                                                close(new_socket_desc);
                                                                exit(1);
                                                        }
                                                }
                                                else if (strcmp(tkn_arr[0], "STOR") == 0)
                                                {
                                                        // printf("%s %lu\n", ip_addr, port_ftp);
                                                        char *cmd1 = strtok(tkn_arr[2], ":"); // first command
                                                        char *cmd2 = strtok(NULL, " \n");     // second command

                                                        strcpy(ip_addr, cmd1);
                                                        port_ftp = atoi(cmd2);
                                                        printf("%s %lu\n", ip_addr, port_ftp);

                                                        strcpy(response, "200 PORT command successful.");
                                                        send(fd, response, sizeof(response), 0);

                                                        // forking the child to excecute LIST
                                                        pid_t child1 = fork();
                                                        if (child1 < 0)
                                                        { /* error occurred */
                                                                fprintf(stderr, "Fork Failed");
                                                                continue;
                                                        }
                                                        else if (child1 == 0)
                                                        { /* child process */
                                                                // child process
                                                                int new_socket_desc;
                                                                struct sockaddr_in new_server_addr, bind_server_addr;
                                                                // Create socket:
                                                                new_socket_desc = socket(AF_INET, SOCK_STREAM, 0);
                                                                if (new_socket_desc < 0)
                                                                {
                                                                        printf("Unable to create socket\n");
                                                                        continue;
                                                                }
                                                                // Set port and IP the same as server-side:
                                                                new_server_addr.sin_family = AF_INET;
                                                                new_server_addr.sin_port = htons(port_ftp);
                                                                new_server_addr.sin_addr.s_addr = inet_addr(ip_addr);

                                                                // Explicitly assigning port number 12010 by
                                                                // binding client with that port
                                                                bind_server_addr.sin_family = AF_INET;
                                                                bind_server_addr.sin_addr.s_addr = inet_addr(ip_addr);
                                                                bind_server_addr.sin_port = htons(2020); // unable to use PORT 20 here for FTP since it is blocked by OS

                                                                if (bind(new_socket_desc, (struct sockaddr *)&bind_server_addr, sizeof(struct sockaddr_in)) == 0)
                                                                        printf("Binded Correctly\n");
                                                                else
                                                                        printf("Unable to bind.\n");

                                                                // Send connection request to server:
                                                                socklen_t addr_size = sizeof new_server_addr;
                                                                if (connect(new_socket_desc, (struct sockaddr *)&new_server_addr, addr_size) < 0)
                                                                {
                                                                        printf("Unable to connect\n");
                                                                        continue;
                                                                }
                                                                printf("Connected with client successfully\n");

                                                                int file_flag;
                                                                recv(new_socket_desc, &file_flag, sizeof(int), 0);
                                                                printf("STOR file flag is %d\n", file_flag);

                                                                if (file_flag)
                                                                {
                                                                        // get the size from the client
                                                                        long size;
                                                                        recv(new_socket_desc, &size, sizeof(long), 0);
                                                                        char *string = malloc(size + 1); // allocating the size of buffer

                                                                        // recieve the data
                                                                        recv(new_socket_desc, string, size, 0);

                                                                        // writing the pwd to a file
                                                                        FILE *writePointer;

                                                                        writePointer = fopen(tkn_arr[1], "wb");
                                                                        if (writePointer == NULL)
                                                                        {
                                                                                printf("Error opening file!\n");
                                                                                exit(1);
                                                                        }

                                                                        // write data to the file
                                                                        fwrite(string, size, 1, writePointer); // write data to the file
                                                                        fflush(writePointer);

                                                                        send(new_socket_desc, "226 Transfer complete", 99, 0);

                                                                        close(new_socket_desc);
                                                                        free(string);
                                                                }
                                                                else
                                                                {
                                                                        printf("File does not exist\n");
                                                                }
                                                                exit(1);
                                                        }
                                                }

                                                else if (strcmp(tkn_arr[0], "RETR") == 0)
                                                {

                                                        char *cmd1 = strtok(tkn_arr[2], ":"); // first command
                                                        char *cmd2 = strtok(NULL, " \n");     // second command

                                                        strcpy(ip_addr, cmd1);
                                                        port_ftp = atoi(cmd2);

                                                        printf("%s %lu\n", ip_addr, port_ftp);

                                                        strcpy(response, "200 PORT command successful.");
                                                        send(fd, response, sizeof(response), 0);

                                                        // forking the child to excecute LIST
                                                        pid_t child1 = fork();

                                                        if (child1 < 0)
                                                        { /* error occurred */
                                                                fprintf(stderr, "Fork Failed");
                                                                continue;
                                                        }

                                                        else if (child1 == 0)
                                                        { /* child process */
                                                                // child process
                                                                int new_socket_desc;
                                                                struct sockaddr_in new_server_addr, bind_server_addr;

                                                                // Create socket:
                                                                new_socket_desc = socket(AF_INET, SOCK_STREAM, 0);

                                                                if (new_socket_desc < 0)
                                                                {
                                                                        printf("Unable to create socket\n");
                                                                        return -1;
                                                                }

                                                                // Set port and IP the same as server-side:
                                                                new_server_addr.sin_family = AF_INET;
                                                                new_server_addr.sin_port = htons(port_ftp);
                                                                new_server_addr.sin_addr.s_addr = inet_addr(ip_addr);
                                                                // inet_addr(ip_addr);

                                                                // Explicitly assigning port number 12010 by
                                                                // binding client with that port
                                                                bind_server_addr.sin_family = AF_INET;
                                                                bind_server_addr.sin_addr.s_addr = inet_addr(ip_addr);
                                                                bind_server_addr.sin_port = htons(2020); // unable to use PORT 20 here for FTP since it is blocked by OS

                                                                // // This ip address will change according to the machine
                                                                //     bind_server_addr.sin_addr.s_addr = INADDR_ANY; //inet_addr(ip_addr);

                                                                if (bind(new_socket_desc, (struct sockaddr *)&bind_server_addr, sizeof(struct sockaddr_in)) == 0)
                                                                        printf("Binded Correctly\n");
                                                                else
                                                                        printf("Unable to bind\n");
                                                                // Send connection request to server:

                                                                socklen_t addr_size = sizeof new_server_addr;
                                                                if (connect(new_socket_desc, (struct sockaddr *)&new_server_addr, addr_size) < 0)
                                                                {
                                                                        printf("Unable to connect\n");
                                                                        return -1;
                                                                }
                                                                printf("Connected with client successfully\n");

                                                                // read the file from the client
                                                                FILE *file;
                                                                char *fileBuffer;
                                                                unsigned long fileLen;

                                                                // Open file
                                                                printf("%s\n", tkn_arr[1]);
                                                                file = fopen(tkn_arr[1], "rb");

                                                                if (file == NULL)
                                                                {
                                                                        int file_flag;
                                                                        fprintf(stderr, "Unable to open file %s", tkn_arr[1]);
                                                                        file_flag = 0;
                                                                        send(new_socket_desc, &file_flag, sizeof(int), 0);
                                                                        // send(new_socket_desc, "Unable to open file", 99, 0);
                                                                        continue;
                                                                }
                                                                else
                                                                {
                                                                        int file_flag;
                                                                        file_flag = 1;
                                                                        send(new_socket_desc, &file_flag, sizeof(int), 0);
                                                                        // getting file length
                                                                        fseek(file, 0, SEEK_END);
                                                                        fileLen = ftell(file);
                                                                        fseek(file, 0, SEEK_SET);

                                                                        // allocating memory
                                                                        fileBuffer = (char *)malloc(fileLen + 1);
                                                                        if (!fileBuffer)
                                                                        {
                                                                                fprintf(stderr, "Memory error!");
                                                                                fclose(file);
                                                                                continue;
                                                                        }

                                                                        // reading file contents to the buffer
                                                                        fread(fileBuffer, fileLen, 1, file);
                                                                        fclose(file);

                                                                        // sending size to the client
                                                                        send(new_socket_desc, &fileLen, sizeof(long), 0);
                                                                        // sending data

                                                                        send(new_socket_desc, fileBuffer, fileLen, 0);

                                                                        close(new_socket_desc);
                                                                        free(fileBuffer);
                                                                }
                                                                exit(1);
                                                        }
                                                }
                                                else if (strcmp(tkn_arr[0], "QUIT") == 0)
                                                {
                                                        strcpy(response, "221 Service closing control connection.");
                                                        send(fd, response, sizeof(response), 0);
                                                        // closing fd when done
                                                        printf("connection closed from client side \n");
                                                        close(fd);
                                                        FD_CLR(fd, &all_socks);
                                                }
                                        }
                                }
                        }
                }
        }
        close(srvr_sock);
        return 0;
}

void *ThreadRun(void *socket)
{
        int *sock = (int *)socket;
        int s = *sock;
        char handShake[255];
        recv(s, &handShake, sizeof(handShake), 0);
        printf("Client with port number: %s connected\n", handShake);
        char hello[255] = {"Hello Client: "};
        send(s, strcat(hello, handShake), strlen(hello), 0);
        close(s);
}