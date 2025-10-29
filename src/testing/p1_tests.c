#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "testing.h"

static tests_t tests[] = {
  {
    .category = "General",
    .prompts = {
      "Program Compiles",
    }
  },
  {
    .category = "Headers",
    .prompts = {
      "Headers are properly formatted",
      "HTTP version is provided",
      "Path matches uesr input",
      "Proper Method is included",
      "Host header exists",
      "User-Agent header exists",
      "Connection header exists",
      "Cache-Control header exists",
      "Accept header exists"
    }
  },
  {
    .category = "Functionality",
    .prompts = {
      "Program calls sans_connect",
      "Program calls sans_send_pkt",
      "Program calls sans_recv_pkt",
      "Program calls sans_disconnect",
      "Program receives and prints small files",
      "Program receives and prints large files",
      "(BONUS) Program defers to Content-Length header",
    }
  }
};

char host[64];
char path[128];
int port;
char* packet1 = "HTTP/1.1 500 Internal Serve Error\r\nCache-Control:max-age=604800\r\nContent-Type: text/html\r\nDate: Sun, 25 Aug 2024 21:19:42 GMT\r\nServer: EOS (vny/0451)\r\nContent-Length: 24\r\nConnection: close\r\n\r\nThis is a dummy packet.\n";

extern int http_client(const char* host, int port);

static void rand_string(char* buf, int len) {
  for (int i=0; i<len; i++) {
    buf[i] = (rand() % 24) + 'a';
  }
}


static int contains(char* a, char* b, int len) {
  int blen = strlen(b);
  for (int i=len-1; i>0; i--) {
    if (strncmp(a + i, b, blen) == 0) {
      return a[i+blen] == '\r' && a[i+blen+1] == '\n';
    }
  }
  return 0;
}

static int validate_header(char* buf, char** ptr, int* len) {
  char* offset = buf;
  while (*offset != '\0' && *offset != '\n')
    offset++;
  if (*offset == '\n')
    offset += 1;
  *ptr = offset;

  int valid = offset[-1] == '\n' && offset[-2] == '\r';
  assert(valid, tests[1].results[0], "FAIL - Malformed header, does not terminate with \\r\\n");
  *len = offset - buf;
  return valid;
}

void first_packet(arg6_t* args) {
  char buf[args->len+1], *header;
  char method[10], p[64], version[10];
  int len, i;
  for (i=0; i<args->len; i++) {
    buf[i] = ((char*)args->buf)[i];
  }
  buf[i] = '\0';
  
  if (!validate_header(buf, &header, &len))
    return;

  sscanf(buf, "%10s %64s %8s", method, p, version);
  
  assert(strcmp(method, "GET") == 0, tests[1].results[3], "FAIL - Method segment does not match expected value");
  assert(strcmp(version, "HTTP/1.1") == 0, tests[1].results[1], "FAIL - HTTP version does not match expected value");
  assert(strcmp(path, p + 1) == 0, tests[1].results[2], "FAIL - Path was not properly formatted in header");
  assert(p[0] == '/', tests[1].results[2], "FAIL - Path did not include a / prefix");

  char hostport[128];
  snprintf(hostport, 128, "%s:%d", host, port);

  while (len) {
    if (strncmp(header, "Host:", 5) == 0) {
      assert(contains(header, hostport, len), tests[1].results[4], "FAIL - Host header does not contain the expected value");
    }
    else if (strncmp(header, "User-Agent:", 11) == 0) {
      assert(contains(header, "sans/1.0", len), tests[1].results[5], "FAIL - User-Agent header does not contain the expected value");
    }
    else if (strncmp(header, "Connection:", 11) == 0) {
      assert(contains(header, "close", len), tests[1].results[6], "FAIL - Connection header does not contain the expected value");
    }
    else if (strncmp(header, "Cache-Control:", 13) == 0) {
      assert(contains(header, "no-cache", len), tests[1].results[7], "FAIL - Cache-Control header does not contain the expected value");
    }
    else if (strncmp(header, "Accept:", 7) == 0) {
      assert(contains(header, "*/*", len), tests[1].results[8], "FAIL - Accept header does not contain the expected value");
    }
    validate_header(header, &header, &len);
  }
  
  for (int i=4; i<8; i++)
    if (strcmp(tests[1].results[i], "UNTESTED") == 0)
      assert(0, tests[1].results[i], "FAIL - Could not identify header");
}

void extra_packet(arg6_t* args) {
  assert(0, tests[2].results[1], "FAIL - Too many packets sent to server");
}

static int pre_sendto(int* result, arg6_t* args) {
  static int pass = 0;
  *result = args->len;

  switch (pass++) {
  case 0: first_packet(args); break;
  default: extra_packet(args); break;
  }
  return -1;
}

static int pre_sans_send(int* result, void* args) {
  assert(1, tests[2].results[1], "");
  return 0;
}

static int pre_sans_recv(int* result, void* args) {
  assert(1, tests[2].results[2], "");
  return 0;
}

static int pre_recvfrom(int* result, arg6_t* args) {
  int len = strlen(packet1);
  len = (len < args->len ? len : args->len);
  for (int i=0; i<len; i++) {
    ((char*)args->buf)[i] = packet1[i];
  }
  *result = len;
  return -1;
}

static int pre_connect(int* result, arg3_conn_t* args) {
  *result = 5;
  assert(args->port == port, tests[2].results[0], "FAIL - Port does not match the command line argument");
  assert(strcmp(args->addr, host) == 0, tests[2].results[0], "FAIL - Address does not match the command line argument");
  return -1;
}

static int pre_disconnect(int* result, void* args) {
  assert(1, tests[2].results[3], "");
  *result = 0;
  return -1;
}

void t__p1_tests(void) {
  char input[133];
  srand(time(NULL));
  rand_string(host, 16);
  port = rand() % 10000;
  s__initialize_tests(tests, 3);
  s__analytics[S_SEND_PKT_REF].precall = (int (*)(int*, void*))pre_sans_send;
  s__analytics[S_RECV_PKT_REF].precall = (int (*)(int*, void*))pre_sans_recv;
  s__analytics[SENDTO_REF].precall = (int (*)(int*, void*))pre_sendto;
  s__analytics[RECVFROM_REF].precall = (int (*)(int*, void*))pre_recvfrom;
  s__analytics[S_CONNECT_REF].precall = (int (*)(int*, void*))pre_connect;
  s__analytics[S_DISCONNECT_REF].precall = pre_disconnect;
  
  rand_string(path, 30);
  snprintf(input, 133, "GET %s\n", path);
  s__spoof_stdin(input, strlen(input));
  
  assert(1, tests[0].results[0], "");
  http_client(host, port);

}
