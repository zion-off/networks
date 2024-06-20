# FTP Implementation in C

## Project Overview

This project implements a simplified version of the FTP application protocol, consisting of two separate programs: an FTP client and an FTP server. The FTP server is responsible for maintaining FTP sessions and providing file access, while the FTP client is split into two components: an FTP user interface and an FTP client to make requests to the FTP server.

## Features

### Server Features
- Supports concurrent connections using `select()` and `fork()`.
- Handles multiple clients simultaneously.
- Provides file access and management.
- Authentication using `USER` and `PASS` commands with credentials stored in `users.txt`.
- Supports the following commands:
  - `USER username`
  - `PASS password`
  - `STOR filename`
  - `RETR filename`
  - `LIST`
  - `CWD foldername`
  - `PWD`
  - `QUIT`

### Client Features
- Provides a simple command-line interface.
- Handles interaction with the FTP server.
- Supports commands:
  - `USER username`
  - `PASS password`
  - `STOR filename`
  - `RETR filename`
  - `LIST`
  - `!LIST`
  - `CWD foldername`
  - `!CWD foldername`
  - `PWD`
  - `!PWD`
  - `QUIT`

## Requirements

- C programming language
- POSIX-compliant system (Linux, macOS)
- GCC compiler

## Installation

1. Clone the repository:
    ```bash
    git clone https://github.com/zion-off/networks.git
    cd ftp-server
    ```

2. Compile the server and client programs:
    ```bash
    gcc server.c -o ftp_server
    gcc client.c -o ftp_client
    ```

3. Ensure the `users.txt` file is in the same directory as the server executable:
    ```text
    bob donuts
    ```

## Usage

### Running the Server
Start the FTP server first:
```bash
./ftp_server
```
The server listens on port 21 and supports multiple client connections.

### Running the Client
Start the FTP client:
```bash
./ftp_client
```

### Commands

#### User Authentication
- **USER username**: Specifies the username for login.
  - Server replies: `331 Username OK, need password.` or `530 Not logged in.`
- **PASS password**: Specifies the password for login.
  - Server replies: `230 User logged in, proceed.` or `530 Not logged in.`

#### File Management
- **STOR filename**: Uploads a file from client to server.
  - Server replies: `150 File status okay; about to open data connection.`, `226 Transfer completed.` or `550 No such file or directory.`
- **RETR filename**: Downloads a file from server to client.
  - Server replies: `150 File status okay; about to open data connection.`, `226 Transfer completed.` or `550 No such file or directory.`
- **LIST**: Lists files in the current server directory.
  - Server replies: `150 File status okay; about to open data connection.`, followed by file list, and `226 Transfer completed.`
- **!LIST**: Lists files in the current client directory.

#### Directory Management
- **CWD foldername**: Changes the server directory.
  - Server replies: `200 Directory changed to pathname/foldername.` or `550 No such file or directory.`
- **!CWD foldername**: Changes the client directory.
- **PWD**: Displays the current server directory.
  - Server replies: `257 pathname.`
- **!PWD**: Displays the current client directory.

#### Session Management
- **QUIT**: Ends the FTP session.
  - Server replies: `221 Service closing control connection.`

## Error Handling
- Invalid commands: Server replies with `202 Command not implemented.`
- Commands issued before authentication: Server replies with `530 Not logged in.`
- Invalid filenames or directories: Server replies with `550 No such file or directory.`
- Invalid command sequences: Server replies with `503 Bad sequence of commands.`

## Notes
- Ensure the server is started before the client.
- The server must be able to read `users.txt` for authentication.
- Data transfers (RETR, STOR, LIST) open a new TCP connection.
- The implementation uses stream transfer mode.