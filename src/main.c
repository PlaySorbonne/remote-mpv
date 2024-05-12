#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>

#define BUFFER_SIZE 1024
#define UNIX_SOCKET_RESPONSE_BUFFER_SIZE 25600
#define TIMEOUT_SECONDS 5

int sockfd = -1;
int connect_sockfd = -1;
int unix_sockfd = -1;

// Custom signal handler function
void sigintHandler(int sig_num) {
  printf("\nCtrl+C pressed. Exiting gracefully...\n");

  if (sockfd!=-1) {
    close(sockfd);
  }
  if (connect_sockfd!=-1) {
    close(connect_sockfd);
  }
  if (unix_sockfd!=-1) {
    close(unix_sockfd);
  }
  exit(EXIT_SUCCESS);
}

char *get_request_body(const char *request) {
  char *body;
  char *bodyStart = strstr(request, "\r\n\r\n");
  if (bodyStart != NULL) {
    bodyStart += 4; // Skip "\r\n\r\n"
    body = malloc(sizeof(char) * (strlen(bodyStart) + 1));
    if (!body) {
      perror("Can not allocate request body string");
      return NULL;
    }

    strcpy(body, bodyStart);
  }
  return body;
}

char *generate_http_response(char * code, char *response_text, char *type) {

  char header[] = "Server: POST BRIDGE\r\n";
  /* "Content-Security-Policy: default-src *\r\n"; */
  /* "Access-Control-Allow-Origin: *\r\n"; */

  size_t response_length = strlen(response_text);
  size_t response_size = response_length * sizeof(char);
  int content_length_size = snprintf(NULL, 0, "%lu", response_size) + 1;
  char *content_length = malloc(content_length_size);
  if (!content_length) {
    perror("Can not determine size of content");
    return NULL;
  }
  snprintf(content_length, content_length_size, "%lu", response_size);

  char *http_response =
      malloc(sizeof(char) *
             (strlen("HTTP/1.0 ") + strlen(code) + strlen(header) + strlen("Content-type: ") + strlen(type) +
              strlen("Content-length: ") + content_length_size +
              strlen("\r\n\r\n") + response_length + 3 * strlen("\r\n") + 1));
  if (!http_response) {
    perror("Can not allocate string for http response");
    free(content_length);
    return NULL;
  }
  strcpy(http_response, "HTTP/1.0 ");
  strcat(http_response, code);
  strcat(http_response, "\r\n");
  strcat(http_response, header);
  strcat(http_response, "Content-type: ");
  strcat(http_response, type);
  strcat(http_response, "\r\n");
  strcat(http_response, "Content-length: ");
  strcat(http_response, content_length);
  strcat(http_response, "\r\n\r\n");
  strcat(http_response, response_text);
  strcat(http_response, "\r\n");
  free(content_length);

  return http_response;
}

int send_message_to_unix_socket(char *unix_socket_path, char *message,
                                char *buffer, size_t buffer_size) {
  struct sockaddr_un server_addr;

  // Create a socket
  if ((unix_sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    return -1;
  }

  // Set up the server address
  server_addr.sun_family = AF_UNIX;
  strncpy(server_addr.sun_path, unix_socket_path,
          sizeof(server_addr.sun_path) - 1);
  server_addr.sun_path[sizeof(server_addr.sun_path) - 1] =
      '\0'; // Ensure null-termination

  // Connect to the mpv IPC socket
  if (connect(unix_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    perror("connect");
    return -1;
  }
  // Set a receive timeout
  struct timeval timeout;
  timeout.tv_sec = TIMEOUT_SECONDS;
  timeout.tv_usec = 0;
  if (setsockopt(unix_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) ==
      -1) {
    perror("setsockopt");
    return -1;
  }

  char *cmd = malloc(sizeof(char) * (strlen(message) + 2));
  if (!cmd) {
    perror("message with \n allocation");
    return -1;
  }
  strcpy(cmd, message);
  strcat(cmd, "\n");
  // Send a command to mpv
  if (send(unix_sockfd, cmd, strlen(cmd), 0) == -1) {
    perror("send");
    free(cmd);
    return -1;
  }
  free(cmd);

  // Receive response from mpv
  int bytes_received;
  if ((bytes_received = recv(unix_sockfd, buffer, buffer_size - 1, 0)) == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      printf("Receive timeout occurred.\n");
      return -1;
    } else {
      perror("recv");
      return -1;
    }
  } else {
    // Null-terminate the received data
    buffer[bytes_received] = '\0';

    // Print the received data
    printf("Response from mpv: %s\n", buffer);
  }

  // Close the socket
  close(unix_sockfd);

  return 0;
}




int main(int argc, char **argv) {

  int portno = 0;
  char *unix_socket_path = NULL;

  // Parse command line options
  int opt;
  while ((opt = getopt(argc, argv, "p:s:")) != -1) {
    switch (opt) {
    case 'p':
      portno = atoi(optarg);
      break;
    case 's':
      unix_socket_path = optarg;
      break;
    default:
      fprintf(stderr, "Usage: %s -p <port> -s <socket_path>\n", argv[0]);
      exit(1);
    }
  }

  if (portno == 0 || unix_socket_path == NULL) {
    fprintf(stderr, "Usage: %s -p <port> -s <unix_socket_path>\n", argv[0]);
    exit(1);
  }
  char buffer[BUFFER_SIZE];

  // Create a socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd == -1) {
    perror("webserver (socket)");
    return 1;
  }
  printf("socket created successfully\n");

  // Create the address to bind the socket to
  struct sockaddr_in host_addr;
  int host_addrlen = sizeof(host_addr);

  host_addr.sin_family = AF_INET;
  host_addr.sin_port = htons(portno);
  host_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  // Create client address
  struct sockaddr_in client_addr;
  int client_addrlen = sizeof(client_addr);

  const int enable = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEPORT) failed");

  // Bind the socket to the address
  if (bind(sockfd, (struct sockaddr *)&host_addr, host_addrlen) != 0) {
    perror("webserver (bind)");
    return 1;
  }
  printf("socket successfully bound to address\n");

  // Listen for incoming connections
  if (listen(sockfd, SOMAXCONN) != 0) {
    perror("webserver (listen)");
    return 1;
  }
  printf("server listening for connections\n");



  // Registering the signal handler for SIGINT
  signal(SIGINT, sigintHandler);

  printf("Press Ctrl+C to exit.\n");

  // Your program logic goes here
  while (true) {
    char *response;
    // Accept incoming connections
    connect_sockfd = accept(sockfd, (struct sockaddr *)&host_addr,
                           (socklen_t *)&host_addrlen);
    if (connect_sockfd < 0) {
      perror("webserver (accept)");
      close(connect_sockfd);
      printf("[%s:%u] Connection closed\n", inet_ntoa(client_addr.sin_addr),
             ntohs(client_addr.sin_port));
      continue;
    }
    printf("connection accepted\n");

    // Get client address
    int sockn = getsockname(connect_sockfd, (struct sockaddr *)&client_addr,
                            (socklen_t *)&client_addrlen);
    if (sockn < 0) {
      perror("webserver (getsockname)");
      close(connect_sockfd);
      printf("[%s:%u] Connection closed\n", inet_ntoa(client_addr.sin_addr),
             ntohs(client_addr.sin_port));
      continue;
    }

    // Read from the socket
    int valread = read(connect_sockfd, buffer, BUFFER_SIZE);
    if (valread < 0) {
      perror("webserver (read)");
      continue;
    }
    if (valread < BUFFER_SIZE) {
      buffer[valread] = '\0';
    } else {
      buffer[BUFFER_SIZE - 1] = '\0';
    }

    // Read the request
    char method[BUFFER_SIZE], uri[BUFFER_SIZE], version[BUFFER_SIZE];
    sscanf(buffer, "%s %s %s", method, uri, version);
    printf("[%s:%u] %s %s %s\n", inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port), method, version, uri);
    /* printf("buffer : %s", buffer); */
    char *body = get_request_body(buffer);
    printf("%s\n", body);
    if (strcmp(uri, "/") == 0) {
      response = generate_http_response("200 OK","<html><h1>REMOTE MPV</h1></html>",
                                        "text/html");
    } else if (strcmp(uri, "/post") == 0) {
      if (body) {
        if (strcmp(method, "POST") == 0) {
          char output[UNIX_SOCKET_RESPONSE_BUFFER_SIZE] = {0};
          size_t buffer_size = sizeof(output);
          int bytes_received = send_message_to_unix_socket(unix_socket_path, body,
                                                           output, buffer_size);
          if (bytes_received < 0) {
            fprintf(stderr, "No response from mpv\n");
            free(body);
            continue;
          }
          response = generate_http_response("200 OK", output, "application/json");
        }
        free(body);
      } else {
        response = generate_http_response("404 Not Found", "NO POST REQUEST", "application/json");
      }
    } else {
      response = generate_http_response("404 Not Found", "NOT FOUND", "application/json");
    }

    if (!response) {
      fprintf(stderr, "Not sending http response\n");
      close(connect_sockfd);
      printf("[%s:%u] Connection closed\n", inet_ntoa(client_addr.sin_addr),
             ntohs(client_addr.sin_port));
      continue;
    }

    // Write to the socket
    int valwrite = write(connect_sockfd, response, strlen(response));
    if (valwrite < 0) {
      perror("webserver (write)");
      close(connect_sockfd);
      printf("[%s:%u] Connection closed\n", inet_ntoa(client_addr.sin_addr),
             ntohs(client_addr.sin_port));
      free(response);
      continue;
    }

    close(connect_sockfd);
    printf("[%s:%u] Connection closed\n", inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));
    free(response);
  }

  return 0;
}
