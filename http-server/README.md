# HTTP/1.0 Server Implementation in C

## Project Overview

The goal of this assignment is to implement an HTTP/1.0 server using TCP socket programming and threading in C. The server responds to HTTP GET requests on TCP port 80, serving HTML, CSS, JavaScript, and image files, while returning a 404 response for unavailable files.

## Features

### Server Features
- Listens on TCP port 80 for HTTP GET requests.
- Serves HTML, CSS, JavaScript, and PNG files.
- Implements concurrent handling of multiple requests using threading.
- Responds with a 404 Not Found for non-existing files.
- Closes the TCP connection after every request and response.
- Provides debug output for each request.

### Supported File Types
- `.html`: Content type `text/html`
- `.css`: Content type `text/css`
- `.js`: Content type `application/javascript`
- `.png`: Content type `image/png`

## Requirements

- C programming language
- POSIX-compliant system (Linux, macOS)
- GCC compiler

## Installation

1. Clone the repository:
    ```bash
    git clone https://github.com/zion-off/networks.git
    cd http-server
    ```

2. Compile the server program:
    ```bash
    gcc http_server.c -o http_server -lpthread
    ```

3. Ensure the necessary files (`index.html`, `style.css`, `script.js`, `logo.png`) are in the same directory as the server executable.

## Usage

1. Start the HTTP server:
    ```bash
    sudo ./http_server
    ```
   Note: Root privileges are required to bind to TCP port 80.

2. Open a web browser and navigate to:
    ```
    http://127.0.0.1/index.html
    ```

3. The server will handle the requests for `index.html` and any referenced files (`style.css`, `script.js`, `logo.png`) concurrently.

### Debug Output

The server will print debug information for each request, similar to:
```
127.0.0.1 [13/Mar/2022 12:23:06] "GET /index.html HTTP/1.1" 200
127.0.0.1 [13/Mar/2022 12:23:08] "GET /script.js HTTP/1.1" 200
127.0.0.1 [13/Mar/2022 12:23:08] "GET /style.css HTTP/1.1" 200
127.0.0.1 [13/Mar/2022 12:23:10] "GET /logo.png HTTP/1.1" 200
```

## HTTP Response Message Format

The HTTP server composes responses according to the HTTP/1.0 specification. Below is an example response for `index.html`:
```
HTTP/1.0 200 OK
Server: SimpleHTTPServer
Date: Wed Mar 23 18:14:21 2022
Content-type: text/html
Content-Length: 353

<html>
   <head>
      <link rel="stylesheet" href="style.css">
      <script src="script.js" type="text/JavaScript"></script>
      <title>My Web page</title>
   </head>
   <body>
      <h1> It works...</h1>
      <img src="logo.png">
      <input type="button" onclick="Hello();" value="Click Me!" />
   </body>
</html>
```

## Error Handling

- Requests for non-existing files return a `404 Not Found` response, including an HTML message indicating the file was not found.
- Example response for a non-existing file (`index.htm`):
```
HTTP/1.0 404 Not Found
Server: SimpleHTTPServer
Date: Wed Mar 23 18:14:21 2022
Content-type: text/html
Content-Length: 112

<html>
   <head><title>404 Not Found</title></head>
   <body>
      <h1>404 Not Found</h1>
      <p>The requested file was not found on this server.</p>
   </body>
</html>
```

## Notes

- Ensure the server is started with root privileges to bind to port 80.
- The server should be able to handle any website with any number of objects and filenames with the supported extensions.
- The implementation treats HTTP/1.1 requests as HTTP/1.0.