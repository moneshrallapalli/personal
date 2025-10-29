#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
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
    .category = "Functionality",
    .prompts = {
      "Program calls sans_accept",
      "Program calls sans_send_pkt",
      "Program calls sans_recv_pkt",
      "Program calls sans_disconnect",
    }
  },
  {
    .category = "Protocol",
    .prompts = {
      "Status line includes version",
      "Status line includes response code",
      "Response includes Content-Length",
      "Response includes Content-Type",
    }
  },
  {
    .category = "Integration",
    .prompts = {
      "Program sends small file correctly",
      "Program sends large file correctly",
    }
  }
};

char* s__testdir = NULL;
static int file_len = 0;
extern int port;
extern char host[64];

extern int http_server(const char*, int);

static void rand_string(char* buf, int len) {
  for (int i=0; i<len; i++) {
    buf[i] = (rand() % 24) + 'a';
  }
}

static int next_header(char* buf) {
  char* ptr;
  for (ptr = buf; *ptr != '\0' && *ptr != '\n'; ptr++);
  if (*ptr == '\n')
    ptr += 1;
  return ptr - buf;
}

static int in_headers = 1;
static int pre_sans_send(int* result, arg3_t* args) {
  char header[args->len];
  char version[10], key[128], val[128], val2[128];
  int code;
  int idx = 0;
  strncpy(header, args->buf, args->len);
  assert(args->socket == 5, tests[1].results[1], "FAIL - Socket was not passed to send");
  assert(args->buf != NULL, tests[1].results[1], "FAIL - Buffer did not point to valid memory");
  assert(args->len <= 1024, tests[1].results[1], "FAIL - Buffer was too large");
  
  if (in_headers) {
    sscanf(header, "%s %d", version, &code);
    assert(strncmp(version, "HTTP/1.1", 8) == 0, tests[2].results[0], "FAIL - Version did not match expected version string");
    assert(strncmp(version, "HTTP", 4) == 0, tests[2].results[0], "FAIL - Status line did not include version");
    assert(code == 200, tests[2].results[1], "FAIL - Status code did not match expected value for this file");
    idx += next_header(&header[idx]);

    while (header[idx] != '\0' && header[idx] != '\r') {
      sscanf(&header[idx], "%128s %128s", key, val);
      if (strncmp(key, "Content-Length:", 15) == 0) {
	int clen = strtol(val, NULL, 10);
	assert(clen == file_len, tests[2].results[2], "FAIL - Content-Length did not match the file's size in bytes");
      }
      else if (strncmp(key, "Content-Type:", 13) == 0) {
	sscanf(&header[idx], "%128s %128s %128s", key, val, val2);
	assert(strncmp(val, "text/html;", 10) == 0, tests[2].results[3], "FAIL - Content-Type did not match expected header value");
	assert(strncmp(val2, "charset=utf-8", 13) == 0, tests[2].results[3], "FAIL - Content-Type did not match expected header value");
      }
      idx += next_header(&header[idx]);
    }
    in_headers = !(header[idx] == '\r');
    for (int i=0; i<4; i++) {
      if (strcmp(tests[2].results[i], "UNTESTED") == 0)
	assert(0, tests[2].results[i], "FAIL - Header was not found in response");
    }
  }

  return 0;
}

static int pre_sans_recv(int* result, arg3_t* args) {
  assert(args->socket == 5, tests[1].results[2], "FAIL - Socket was not passed to recv");
  assert(args->buf != NULL, tests[1].results[2], "FAIL - Buffer did not point to valid memory");
  assert(args->len <= 1024, tests[1].results[2], "FAIL - Buffer length was too large");
  return 0;
}

static int pre_accept(int* result, arg3_conn_t* args) {
  *result = 5;
  assert(args->port == port, tests[1].results[0], "FAIL - Port does not match the command line argument");
  assert(strcmp(args->addr, host) == 0, tests[1].results[0], "FAIL - Address does not match the command line argument");
  return -1;
}

static int pre_disconnect(int* result, arg1_t* args) {
  *result = 0;
  assert(args->socket == 5, tests[1].results[3], "FAIL - Socket was not passed to disconnect");
  return -1;
}

static int packet_out_num = 0;
static int pre_sendto_static(int* result, arg6_t* args) {
  packet_out_num++;
  *result = args->len;
  return -1;
}

static int pre_sendto_param(int* result, arg6_t* args) {
  char path[1024];
  FILE* fp;
  snprintf(path, 1024, "%s/%d.out", s__testdir, packet_out_num++);

  fp = fopen(path, "w");
  fwrite(args->buf, sizeof(char), args->len, fp);
  *result = args->len;
  fclose(fp);
  
  return -1;
}

static int packet_in_num = 0;
static char* static_data[] = {
  "GET /.test.html HTTP/1.1\r\n"
  "Host: localhost:8888\r\n"
  "User-Agent: sans-test/1.0\r\n"
  "Cache-Control: no-cache\r\n"
  "Connection: close\r\n"
  "Accept: */*\r\n"
  "\r\n",
};
static int pre_recvfrom_static(int* result, arg6_t* args) {
  while (packet_in_num > 0);
  *result = strlen(static_data[packet_in_num]);
  strncpy((char*)args->buf, static_data[packet_in_num++], args->len);
  return -1;
}

static int pre_recvfrom_param(int* result, arg6_t* args) {
  char path[1024];
  FILE* fp;
  snprintf(path, 1024, "%s/%d.in", s__testdir, packet_in_num++);

  fp = fopen(path, "r");
  *result = fread((char*)args->buf, sizeof(char), args->len, fp);
  fclose(fp);

  if (packet_in_num == 1) {
    char path[128];
    struct stat s_stat;
    sscanf(args->buf, "GET /%s", path);
    if (stat(path, &s_stat)) {
      fprintf(stderr, "ERROR - Failed to open file for stat\n");
    }
    file_len = s_stat.st_size;
  }
  return -1;
}

void t__p2_tests(void) {
  FILE* fp;
  char out[1024], err[1024];
  srand(time(NULL));
  rand_string(host, 16);
  port = (rand() % 8000) + 1024;
  s__initialize_tests(tests, 4);
  
  s__analytics[S_SEND_PKT_REF].precall   = (int (*)(int*, void*))pre_sans_send;
  s__analytics[S_RECV_PKT_REF].precall   = (int (*)(int*, void*))pre_sans_recv;
  s__analytics[S_ACCEPT_REF].precall     = (int (*)(int*, void*))pre_accept;
  s__analytics[S_DISCONNECT_REF].precall = (int (*)(int*, void*))pre_disconnect;

  assert(1, tests[0].results[0], "");

  if (s__testdir == NULL) {
    fp = fopen(".test.html", "w");
    fwrite("This is a test", sizeof(char), 14, fp);
    file_len = 14;
    fclose(fp);
    s__analytics[SENDTO_REF].precall       = (int (*)(int*, void*))pre_sendto_static;
    s__analytics[RECVFROM_REF].precall     = (int (*)(int*, void*))pre_recvfrom_static;
#ifdef HEADLESS
    s__dump_stdout("test.out", "test.err");
#else
    s__dump_stdout();
#endif
  }
  else {
    snprintf(out, 1024, "%s/stdout", s__testdir);
    snprintf(err, 1024, "%s/stderr", s__testdir);
    s__analytics[SENDTO_REF].precall       = (int (*)(int*, void*))pre_sendto_param;
    s__analytics[RECVFROM_REF].precall     = (int (*)(int*, void*))pre_recvfrom_param;
#ifdef HEADLESS
    s__dump_stdout(out, err);
#else
    s__dump_stdout();
#endif
  }

  http_server(host, port);
  
  assert(strcmp(tests[1].results[1], "UNTESTED") != 0, tests[1].results[1], "FAIL - No calls made to sans_send");
  assert(strcmp(tests[1].results[0], "UNTESTED") != 0, tests[1].results[0], "FAIL - No calls made to sans_accept");
  assert(strcmp(tests[1].results[3], "UNTESTED") != 0, tests[1].results[3], "FAIL - No calls made to sans_disconnect");

  for (int i=0; i<4; i++)
    assert(strcmp(tests[1].results[i], "UNTESTED") != 0, tests[1].results[i], "FAIL - No headers read, could not perform test");
  
  assert(packet_in_num > 0, tests[1].results[2], "FAIL - No calls made to sans_recv");
  assert(packet_in_num < 2, tests[1].results[2], "FAIL - Too many calls to sans_recv");
}
