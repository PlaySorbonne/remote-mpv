/*
 * This program is a simple HTTP server that listens for incoming connections
 * and responds with appropriate HTTP responses. It communicates with a Unix
 * socket to process POST requests and generate responses based on the received
 * data. The server also handles SIGINT (Ctrl+C) gracefully by closing all open
 * sockets before exiting.
 *
 * Author: Loïc Daudé Mondet
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <magic.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define ANSI_RESET_ALL "\x1b[0m"
#define ANSI_COLOR_BLACK "\x1b[30m"
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_WHITE "\x1b[37m"
#define ANSI_BACKGROUND_BLACK "\x1b[40m"
#define ANSI_BACKGROUND_RED "\x1b[41m"
#define ANSI_BACKGROUND_GREEN "\x1b[42m"
#define ANSI_BACKGROUND_YELLOW "\x1b[43m"
#define ANSI_BACKGROUND_BLUE "\x1b[44m"
#define ANSI_BACKGROUND_MAGENTA "\x1b[45m"
#define ANSI_BACKGROUND_CYAN "\x1b[46m"
#define ANSI_BACKGROUND_WHITE "\x1b[47m"
#define ANSI_STYLE_BOLD "\x1b[1m"
#define ANSI_STYLE_ITALIC "\x1b[3m"
#define ANSI_STYLE_UNDERLINE "\x1b[4m"

#define BUFFER_SIZE 1024
#define UNIX_SOCKET_RESPONSE_BUFFER_SIZE 25600
#define TIMEOUT_SECONDS 5
#define CONTENT_ERROR_404                                                      \
  "<html><head><title>Resource Not Found</title></head><body><p>The resource " \
  "you requested has not been found at the specified address. Please check "   \
  "the spelling of the address.</p></body></html>"
#define DEFAULT_MIME_TYPE "text/html"

// Global variables for socket file descriptors
int sockfd = -1;
int connect_sockfd = -1;
int unix_sockfd = -1;
magic_t magic_cookie = NULL;

// Signal handler for SIGINT (Ctrl+C)
void sigint_handler(int sig_num) {
  (void)sig_num;
  printf(ANSI_STYLE_BOLD ANSI_COLOR_RED
         "\nCtrl+C pressed. Exiting gracefully...\n" ANSI_RESET_ALL);

  // Close all open sockets before exiting
  if (sockfd != -1)
    close(sockfd);
  if (connect_sockfd != -1)
    close(connect_sockfd);
  if (unix_sockfd != -1)
    close(unix_sockfd);
  if (magic_cookie)
    magic_close(magic_cookie);

  exit(EXIT_SUCCESS);
}

// Extracts the request body from HTTP request
char *get_request_body(const char *request) {
  char *body = strstr(request, "\r\n\r\n");
  if (body != NULL) {
    body += 4; // Skip "\r\n\r\n"
    return strdup(body);
  }
  return NULL;
}

// Generates HTTP response
char *generate_http_response(const char *code, const char *response_text,
                             const char *type) {
  // Calculate the size needed for http_response
  size_t response_length = strlen(response_text);
  size_t http_response_size =
      sizeof(char) *
      (strlen("HTTP/1.1 \r\n") + strlen(code) +
       strlen("Server: UNIX SOCKET BRIDGE\r\n") + strlen("Content-type: \r\n") +
       strlen(type) +
       strlen(
           "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: "
           "GET, POST, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type, "
           "Authorization\r\nAccess-Control-Allow-Credentials: true\r\n") +
       strlen("Content-length: \r\n") +
       snprintf(NULL, 0, "%lu", response_length) + strlen("\r\n\r\n") +
       response_length + strlen("\r\n") + 1);

  // Alocate response string
  char *http_response = malloc(http_response_size);
  if (!http_response) {
    perror("Can not allocate string for http response");
    return NULL;
  }

  // Format the http_response string
  snprintf(http_response, http_response_size,
           "HTTP/1.1 %s\r\n"
           "Server: UNIX SOCKET BRIDGE\r\n"
           "Content-type: %s\r\n"
           "Access-Control-Allow-Origin: *\r\n"
           "Access-Control-Allow-Methods: GET, POST, OPTION\r\n"
           "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
           "Access-Control-Allow-Credentials: true\r\n"
           "Content-length: %lu\r\n"
           "\r\n%s\r\n",
           code, type, response_length, response_text);

  return http_response;
}

// Sends message to Unix socket and receives response
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

  // Setting the receive timeout
  struct timeval timeout;
  timeout.tv_sec = TIMEOUT_SECONDS; // 5 seconds timeout
  timeout.tv_usec = 0;              // 0 microseconds

  if (setsockopt(unix_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                 sizeof(timeout)) < 0) {
    perror("setsockopt");
    close(sockfd);
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
      fprintf(stderr, "Receive timeout occurred.\n");
      return -1;
    } else {
      perror("recv");
      return -1;
    }
  } else {
    buffer[bytes_received] = '\0';
    printf(ANSI_COLOR_CYAN "Response from UNIX SOCKET: " ANSI_RESET_ALL "%s\n",
           buffer);
  }

  close(unix_sockfd);
  return 0;
}

bool readFile(const char *filename, char **content) {
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    fprintf(stderr, "Error opening file %s\n", filename);
    return false;
  }

  fseek(file, 0, SEEK_END);
  unsigned long file_size = ftell(file);
  rewind(file);

  *content = (char *)malloc(file_size + 1);
  if (*content == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    fclose(file);
    return false;
  }

  unsigned long result = fread(*content, 1, file_size, file);
  if (result != file_size) {
    fprintf(stderr, "Error reading file %s\n", filename);
    fclose(file);
    free(*content);
    return false;
  }

  (*content)[file_size] = '\0';

  fclose(file);

  return true;
}

bool fileExists(const char *filename) { return access(filename, F_OK) != -1; }

bool hasExtension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    return dot != NULL && dot != filename; // Ensure dot is not at the beginning
}

int main(int argc, char **argv) {
  int port_nb = 0;
  char *unix_socket_path = NULL;
  char *web_server_dir = NULL; // Variable to store the directory path
  char buffer[BUFFER_SIZE];

  // Parse command line arguments for port number, Unix socket path, and
  // directory path
  int opt;
  while ((opt = getopt(argc, argv, "p:s:d:")) != -1) {
    switch (opt) {
    case 'p':
      port_nb = atoi(optarg);
      break;
    case 's':
      unix_socket_path = optarg;
      break;
    case 'd':
      web_server_dir = optarg;
      break;
    default:
      fprintf(stderr,
              "Usage: %s -p <port> -s <unix_socket_path> -d "
              "<web_server_directory>\n",
              argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  // Ensure port number, Unix socket path, and web server directory are provided
  if (port_nb == 0 || unix_socket_path == NULL || web_server_dir == NULL) {
    fprintf(
        stderr,
        "Usage: %s -p <port> -s <unix_socket_path> -d <web_server_directory>\n",
        argv[0]);
    exit(EXIT_FAILURE);
  }

  // Create magic database
  magic_cookie = magic_open(MAGIC_MIME_TYPE);
  if (magic_cookie == NULL) {
    fprintf(stderr, "Unable to initialize magic library\n");
    return 1;
  }

  // Load the default magic database
  if (magic_load(magic_cookie, NULL) != 0) {
    fprintf(stderr, "Cannot load magic database - %s\n",
            magic_error(magic_cookie));
    magic_close(magic_cookie);
    return 1;
  }

  // Create TCP socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  // Configure socket address for binding
  struct sockaddr_in host_addr = {0}, client_addr = {0};
  socklen_t host_addrlen = sizeof(host_addr),
            client_addrlen = sizeof(client_addr);
  int enable = 1;

  // Allow socket reuse
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEPORT) failed");

  // Bind socket to address and port
  host_addr.sin_family = AF_INET;
  host_addr.sin_port = htons(port_nb);
  host_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sockfd, (struct sockaddr *)&host_addr, host_addrlen) != 0) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  // Listen for incoming connections
  if (listen(sockfd, SOMAXCONN) != 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  // Register signal handler for SIGINT
  signal(SIGINT, sigint_handler);
  printf(ANSI_STYLE_BOLD ANSI_COLOR_RED
         "Press Ctrl+C to exit.\n" ANSI_RESET_ALL);

  // Main server loop
  while (true) {
    char *response;
    connect_sockfd =
        accept(sockfd, (struct sockaddr *)&client_addr, &client_addrlen);
    if (connect_sockfd < 0) {
      perror("accept");
      continue;
    }

    // Setting the receive timeout
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SECONDS; // 5 seconds timeout
    timeout.tv_usec = 0;              // 0 microseconds

    if (setsockopt(connect_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                   sizeof(timeout)) < 0) {
      perror("setsockopt");
      close(sockfd);
      return 1;
    }

    int n_read = read(connect_sockfd, buffer, BUFFER_SIZE);
    if (n_read < 0) {
      perror("read");
      close(connect_sockfd);
      continue;
    }
    buffer[n_read] = '\0';

    char method[BUFFER_SIZE], uri[BUFFER_SIZE], version[BUFFER_SIZE];
    sscanf(buffer, "%s %s %s", method, uri, version);
    printf(ANSI_COLOR_GREEN "[%s:%u] " ANSI_RESET_ALL ANSI_COLOR_YELLOW
                            "%s %s %s\n" ANSI_RESET_ALL,
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), method,
           version, uri);

    char *body = get_request_body(buffer);
    printf("%s\n", body);

    // Generate appropriate HTTP response based on requested URI
    if (strcmp(uri, "/") == 0) {
      if (strcmp(method, "GET") == 0) {
        // Create a string composed of web_server_dir concatenated with a
        // filename
        char *filename =
            "index.html"; // You can change this to any other filename

        // Calculate the length of the filepath
        size_t web_server_dir_len = strlen(web_server_dir);
        size_t filename_len = strlen(filename);
        size_t filepath_len =
            web_server_dir_len + filename_len +
            2; // +1 for the '/' separator, +1 for the null terminator

        // Allocate memory for the filepath dynamically
        char *filepath = (char *)malloc(filepath_len);
        if (filepath == NULL) {
          fprintf(stderr, "Memory allocation failed\n");
          free(body);
          continue;
        }

        // Concatenate web_server_dir and filename into filepath
        snprintf(filepath, filepath_len, "%s/%s", web_server_dir, filename);
        if (fileExists(filepath)) {
          // Use magic database to detect MIME type
          const char *mime_type = magic_file(magic_cookie, filepath);
          if (mime_type == NULL) {
            fprintf(stderr, "Cannot determine MIME type - %s\n",
                    magic_error(magic_cookie));
            mime_type = DEFAULT_MIME_TYPE; // Default to text/html on error
          }

          char *fileContent;
          bool success = readFile(filepath, &fileContent);
          if (success) {
            response = generate_http_response("200 OK", fileContent, mime_type);
            free(fileContent);
          } else {
            response = generate_http_response("404 Not Found",
                                              CONTENT_ERROR_404, "text/html");
          }
        } else {
          response = generate_http_response("404 Not Found", CONTENT_ERROR_404,
                                            "text/html");
        }

        free(filepath);
      } else {
        response = generate_http_response("404 Not Found", CONTENT_ERROR_404,
                                          "text/html");
      }
    } else if (strcmp(uri, "/post") == 0) {
      if (body) {
        if (strcmp(method, "POST") == 0) {
          char output[UNIX_SOCKET_RESPONSE_BUFFER_SIZE] = {0};
          size_t buffer_size = sizeof(output);

          // Send message to Unix socket and receive response
          int bytes_received = send_message_to_unix_socket(
              unix_socket_path, body, output, buffer_size);
          if (bytes_received < 0) {
            fprintf(stderr, "No response from UNIX SOCKET\n");
            free(body);
            close(connect_sockfd);
            continue;
          }
          response =
              generate_http_response("200 OK", output, "application/json");
        } else {
          response = generate_http_response("404 Not Found", CONTENT_ERROR_404,
                                            "text/html");
        }
      } else {
        response = generate_http_response("404 Not Found", CONTENT_ERROR_404,
                                          "text/html");
      }
    } else {
      if (strcmp(method, "GET") == 0) {
        // Create a string composed of web_server_dir concatenated with a
        // filename
        char *filename = uri; // You can change this to any other filename


        char extension[6];
        // Calculate the length of the filepath
        size_t web_server_dir_len = strlen(web_server_dir);
        size_t filename_len = strlen(filename);
        size_t filepath_len =
            web_server_dir_len + filename_len +
            1; // +1 for the null terminator
char *filepath;
         if (!hasExtension(filename)) {
            filepath_len += 6;

        extension[0] = '.';
        extension[1] = 'h';
        extension[2] = 't';
        extension[3] = 'm';
        extension[4] = 'l';
        extension[5] = '\0';


         } else {
           extension[0]='\0';

        }
                            // Allocate memory for the filepath dynamically
        filepath = (char *)malloc(filepath_len);
        if (filepath == NULL) {
          fprintf(stderr, "Memory allocation failed\n");
          free(body);
          continue;
        }

                // Concatenate web_server_dir and filename into filepath
        snprintf(filepath, filepath_len, "%s%s%s", web_server_dir, filename, extension);
        if (fileExists(filepath)) {
          // Use magic database to detect MIME type
          const char *mime_type = magic_file(magic_cookie, filepath);
          if (mime_type == NULL) {
            fprintf(stderr, "Cannot determine MIME type - %s\n",
                    magic_error(magic_cookie));
            mime_type = DEFAULT_MIME_TYPE; // Default to text/html on error
          }

          char *fileContent;
          bool success = readFile(filepath, &fileContent);
          if (success) {
            response = generate_http_response("200 OK", fileContent, mime_type);
            free(fileContent);
          } else {
            response = generate_http_response("404 Not Found",
                                              CONTENT_ERROR_404, "text/html");
          }
        } else {
          response = generate_http_response("404 Not Found", CONTENT_ERROR_404,
                                            "text/html");
        }
        free(filepath);
      } else {
        response = generate_http_response("404 Not Found", CONTENT_ERROR_404,
                                          "text/html");
      }
    }
    free(body);

    if (!response) {
      fprintf(stderr, "Failed to generate HTTP response\n");
      close(connect_sockfd);
      continue;
    }

    // Send HTTP response to client
    int n_written = write(connect_sockfd, response, strlen(response));
    if (n_written < 0) {
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
