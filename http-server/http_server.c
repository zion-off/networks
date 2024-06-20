
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define PORT 8080
#define MAX_BUFFER_SIZE 1024

void *ThreadRun(void *);
char *parse_http_request(char *);
char *get_content_type(char *);
void send_file(int, char *, char *);

int main(int argc, char const *argv[])
{
    int server_fd, new_socket;
    struct sockaddr_in address, new_socket_addr;
    int addrlen = sizeof(new_socket_addr);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("In socket");
        exit(EXIT_FAILURE);
    }

    memset(address.sin_zero, '\0', sizeof address.sin_zero);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("In bind");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) < 0)
    {
        perror("In listen");
        exit(EXIT_FAILURE);
    }
    while (1)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&new_socket_addr, (socklen_t *)&addrlen)) < 0)
        {
            perror("In accept");
            exit(EXIT_FAILURE);
        }

        int *socket_ptr = malloc(sizeof(int));

        *socket_ptr = new_socket;

        pthread_t th;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&th, &attr, ThreadRun, socket_ptr);
    }
    return 0;
}

void *ThreadRun(void *socket)
{
    int *sock = (int *)socket;
    int s = *sock;

    char buffer[1024];
    ssize_t bytes_received = recv(s, buffer, sizeof(buffer), 0);

    if (bytes_received < 0)
    {
        perror("In ThreadRun");
        exit(EXIT_FAILURE);
    }

    else
    {
        buffer[bytes_received] = '\0';

        // parse the request to get the requested file path
        char *requested_file = parse_http_request(buffer);

        // check the requested file extension
        char content_type[MAX_BUFFER_SIZE];

        // special case: if the requested file is "/", then serve index.html
        if (strcmp(requested_file, "index.html") == 0)
        {
            strcpy(content_type, "text/html");
        }
        // otherwise, get the content type based on the file extension
        else
        {
            strcpy(content_type, get_content_type(requested_file));
        }

        // get the client's IP address
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        getpeername(s, (struct sockaddr *)&client_addr, &addr_len);
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

        // get the current time
        time_t t;
        struct tm *tm_info;
        char formatted_time[80];
        time(&t);
        tm_info = localtime(&t);
        strftime(formatted_time, sizeof(formatted_time), "%d/%b/%Y %H:%M:%S", tm_info);

        // print the client's IP address & the current time
        printf("%s [%s] ", client_ip, formatted_time);

        // get the first line of the request
        // eg "GET /index.html HTTP/1.0"
        char first_line[MAX_BUFFER_SIZE];
        int i;
        for (i = 0; buffer[i] != '\n' && buffer[i] != '\r'; i++)
        {
            first_line[i] = buffer[i];
        }

        // if get_content_type returned "404", then the file type is unknown
        if (strcmp(content_type, "404") == 0)
        {
            printf("\"%s\"", first_line);
            printf(" 404\n");
        }
        // otherwise, file was found and is beign sent
        else
        {
            printf("\"%s\"", first_line);
            printf(" 200\n");
        }

        // open and send the file to the socket
        send_file(s, requested_file, content_type);
    }

    free(socket);

    return NULL;
}

char *parse_http_request(char *request)
{
    // find the start of the requested file path by looking for "GET"
    char *get_start = strstr(request, "GET ");

    // if for example, the request is "GET /index.html HTTP/1.0"
    // then file_start will point to the file name
    // read until the first space to get the file name
    char *file_start = get_start + 4;
    char *file_end = strpbrk(file_start, " \r\n");

    size_t file_length = file_end - file_start;
    char *requested_file = (char *)malloc(file_length + 1);
    strncpy(requested_file, file_start, file_length);
    requested_file[file_length] = '\0';

    // special case: if the requested file is "/", then serve index.html
    if (strcmp(requested_file, "/") == 0)
    {
        char *file_name = "index.html";
        return file_name;
    }

    return requested_file;
}

char *get_content_type(char *file_path)
{
    if (strstr(file_path, ".html") != NULL)
    {
        return "text/html";
    }
    else if (strstr(file_path, ".css") != NULL)
    {
        return "text/css";
    }
    else if (strstr(file_path, ".js") != NULL)
    {
        return "application/javascript";
    }
    else if (strstr(file_path, ".png") != NULL)
    {
        return "image/png";
    }
    else
    {
        // unknown file type
        return "404";
    }
}

void send_file(int socket, char *file_path, char *content_type)
{
    int file_fd;


    if (strcmp(file_path, "index.html") != 0)
    {
        file_fd = open(file_path + 1, O_RDONLY);
    }
    else
    {
        file_fd = open(file_path, O_RDONLY);
    }

    // get the file size
    struct stat file_stat;
    fstat(file_fd, &file_stat);
    off_t file_size = file_stat.st_size;

    // get the current time
    time_t t;
    struct tm *tm_info;
    char formatted_time[80];
    time(&t);
    tm_info = localtime(&t);
    strftime(formatted_time, sizeof(formatted_time), "%d/%b/%Y %H:%M:%S", tm_info);

    if (file_fd == -1)
    {
        // if the file is not found, send a 404 Not Found response
        char not_found_response[1024];
        sprintf(not_found_response, "HTTP/1.0 404 Not Found\r\nServer: SimpleHTTPServer\r\nDate: %s\r\nContent-Type: text/html\r\n\r\n<html><body><h1>404 Not Found</h1></body></html>", formatted_time);
        send(socket, not_found_response, strlen(not_found_response), 0);

        perror("Unable to open file");
        return;
    }

    // if the file is found, send a 200 OK response
    char response[1024];

    // send the response header
    sprintf(response, "HTTP/1.0 200 OK\r\nServer: SimpleHTTPServer\r\nDate: %s\r\nContent-Type: %s\r\nContent-Length: %lld\r\n\r\n", formatted_time, content_type, (long long)file_size);
    send(socket, response, strlen(response), 0);
    char buffer[1024];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0)
    {
        send(socket, buffer, bytes_read, 0);
    }

    close(file_fd);

    shutdown(socket, SHUT_WR);

    return;
}
