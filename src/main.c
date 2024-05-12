#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define UNIX_SOCKET_RESPONSE_BUFFER_SIZE 25600
#define TIMEOUT_SECONDS 5

int sockfd = -1;
int connect_sockfd = -1;
int unix_sockfd = -1;

void sigintHandler(int sig_num) {
  printf("\nCtrl+C pressed. Exiting gracefully...\n");

  if (sockfd != -1)
    close(sockfd);
  if (connect_sockfd != -1)
    close(connect_sockfd);
  if (unix_sockfd != -1)
    close(unix_sockfd);

  exit(EXIT_SUCCESS);
}

char *get_request_body(const char *request) {
  char *body = strstr(request, "\r\n\r\n");
  if (body != NULL) {
    body += 4; // Skip "\r\n\r\n"
    return strdup(body);
  }
  return NULL;
}

char *generate_http_response(const char *code, const char *response_text,
                             const char *type) {
  char *http_response = malloc(BUFFER_SIZE);
  if (!http_response) {
    perror("Can not allocate string for http response");
    return NULL;
  }
  snprintf(http_response, BUFFER_SIZE,
           "HTTP/1.0 %s\r\n"
           "Server: POST BRIDGE\r\n"
           "Content-type: %s\r\n"
           "Content-length: %lu\r\n"
           "\r\n%s\r\n",
           code, type, strlen(response_text), response_text);
  return http_response;
}

int send_message_to_unix_socket(const char *unix_socket_path,
                                const char *message, char *buffer,
                                size_t buffer_size) {
  struct sockaddr_un server_addr;
  ssize_t bytes_received;

  unix_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (unix_sockfd == -1) {
    perror("socket");
    return -1;
  }

  server_addr.sun_family = AF_UNIX;
  strncpy(server_addr.sun_path, unix_socket_path,
          sizeof(server_addr.sun_path) - 1);

  if (connect(unix_sockfd, (struct sockaddr *)&server_addr,
              sizeof(server_addr)) == -1) {
    perror("connect");
    return -1;
  }

  char *cmd = malloc(strlen(message) + 2);
  if (!cmd) {
    perror("message with newline allocation");
    return -1;
  }
  strcpy(cmd, message);
  strcat(cmd, "\n");

  if (send(unix_sockfd, cmd, strlen(cmd), 0) == -1) {
    perror("send");
    free(cmd);
    return -1;
  }
  free(cmd);

  bytes_received = recv(unix_sockfd, buffer, buffer_size - 1, 0);
  if (bytes_received == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      printf("Receive timeout occurred.\n");
      return -1;
    } else {
      perror("recv");
      return -1;
    }
  } else {
    buffer[bytes_received] = '\0';
    printf("Response from mpv: %s\n", buffer);
  }

  close(unix_sockfd);
  return 0;
}

int main(int argc, char **argv) {
  int portno = 0;
  char *unix_socket_path = NULL;
  char buffer[BUFFER_SIZE];

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
      fprintf(stderr, "Usage: %s -p <port> -s <unix_socket_path>\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (portno == 0 || unix_socket_path == NULL) {
    fprintf(stderr, "Usage: %s -p <port> -s <unix_socket_path>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in host_addr = {0}, client_addr = {0};
  socklen_t host_addrlen = sizeof(host_addr),
            client_addrlen = sizeof(client_addr);
  int enable = 1;

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEPORT) failed");

  host_addr.sin_family = AF_INET;
  host_addr.sin_port = htons(portno);
  host_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sockfd, (struct sockaddr *)&host_addr, host_addrlen) != 0) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  if (listen(sockfd, SOMAXCONN) != 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, sigintHandler);
  printf("Press Ctrl+C to exit.\n");

  while (true) {
    char *response;
    connect_sockfd =
        accept(sockfd, (struct sockaddr *)&client_addr, &client_addrlen);
    if (connect_sockfd < 0) {
      perror("accept");
      continue;
    }

    int valread = read(connect_sockfd, buffer, BUFFER_SIZE);
    if (valread < 0) {
      perror("read");
      close(connect_sockfd);
      continue;
    }
    buffer[valread] = '\0';

    char method[BUFFER_SIZE], uri[BUFFER_SIZE], version[BUFFER_SIZE];
    sscanf(buffer, "%s %s %s", method, uri, version);
    printf("[%s:%u] %s %s %s\n", inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port), method, version, uri);

    char *body = get_request_body(buffer);
    printf("%s\n", body);

    if (strcmp(uri, "/") == 0) {
      response = generate_http_response(
          "200 OK", "<html><h1>REMOTE MPV</h1></html>", "text/html");
    } else if (strcmp(uri, "/post") == 0) {
      if (body) {
        if (strcmp(method, "POST") == 0) {
          char output[UNIX_SOCKET_RESPONSE_BUFFER_SIZE] = {0};
          size_t buffer_size = sizeof(output);
          int bytes_received = send_message_to_unix_socket(
              unix_socket_path, body, output, buffer_size);
          if (bytes_received < 0) {
            fprintf(stderr, "No response from mpv\n");
            free(body);
            continue;
          }
          response =
              generate_http_response("200 OK", output, "application/json");
        }
        free(body);
      } else {
        response = generate_http_response("404 Not Found", "NO POST REQUEST",
                                          "application/json");
      }
    } else {
      response = generate_http_response("404 Not Found", "NOT FOUND",
                                        "application/json");
    }

    if (!response) {
      fprintf(stderr, "Failed to generate HTTP response\n");
      close(connect_sockfd);
      continue;
    }

    int valwrite = write(connect_sockfd, response, strlen(response));
    if (valwrite < 0) {
      perror("write");
      close(connect_sockfd);
      free(response);
      continue;
    }

    close(connect_sockfd);
    free(response);
  }

  return 0;
}
