#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "include/sans.h"



void print_help(int code) {
  char* help_text =
    "\nusage - sans <protocol> <type> <host> <port>\n"
    "\n"
    "protocol - (http|smtp)\n"
    "    Indicates the communication protocol run by the program.\n"
    "    - When set to 'http' the program will send/receive a file\n"
    "      using the 'http' protocol.\n"
    "    - When set to 'smtp' the program will send an email to an\n"
    "      smtp agent.\n"
    "type - (client|server|proxy)\n"
    "    Indicates the type of process to run.\n"
    "    - When set to 'client' the program will  try to connect\n"
    "      to a  remote  server and  run the requested  protocol.\n"
    "    - When set to 'server' the  program will  listen for an\n"
    "      incoming connection and respond to requests using the\n"
    "      protocol.\n"
    "    - When set to 'proxy' the program will multiplex any incomming\n"
    "      TCP connections between a selection of servers.\n"
    "host\n"
    "    The hostname or IP to connect to.  The value will be applied\n"
    "    depending on the 'type' argument.\n"
    "    - If run as a client, this argument indicates the hostname or IP\n"
    "      of the destination server.\n"
    "    - If run as a server, this argument indicates which address the\n"
    "      server should be reachable at."
    "port\n"
    "  The port number used by the application\n";

  printf("%s\n", help_text);
  exit(code);
}

int error_agent(const char* host, int port) {
  printf("[ERROR] Cannot run 'smtp' in 'server' mode\n");
  return 0;
}

int error_proxy(const char* host, int port) {
  printf("[ERROR] Cannot run 'smtp' in 'proxy' mode\n");
  return 0;
}

void choose_type(const char* type, const char* host, int port, int (*client)(const char*, int), int (*server)(const char*, int), int (*proxy)(const char*, int)) {
  if (strcmp(type, "client") == 0) {
    client(host, port);
  }
  else if (strcmp(type, "server") == 0) {
    server(host, port);
  }
  else if (strcmp(type, "proxy") == 0) {
    proxy(host, port);
  }
  else {
    printf("[ERROR] Unknown type `%s`, must be one of \"server\" or \"client\"\n", type);
    print_help(-2);
  }
}

void* sans_backend(void*);
int main(int argc, char** argv) {
  if (argc >= 2)
    for (int i=1; i < argc; i++)
      if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
	print_help(0);
  if (argc != 5) {
    if (argc != 5)
      printf("[ERROR] Incorrect arguments");
    print_help(-1);
  }
  int port = strtol(argv[4], NULL, 0);

  { 
    int init_rudp_backend(void);
    if (init_rudp_backend() != 0) {
      fprintf(stderr, "Error returned during rudp backend setup\n");
      exit(-2);
    }

    pthread_t backend_thread;
    int result = pthread_create(&backend_thread, NULL, rudp_backend, NULL);
    if (result != 0) {
      fprintf(stderr, "Failed to create background worker thread\n");
      exit(-1);
    }
  }
  
  if (strcmp(argv[1], "http") == 0)
    choose_type(argv[2], argv[3], port, http_client, http_server, tcp_proxy);
  else if (strcmp(argv[1], "smtp") == 0)
    choose_type(argv[2], argv[3], port, smtp_agent, error_agent, error_proxy);

  return 0;
}
