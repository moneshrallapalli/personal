#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include "testing.h"

#define SOCKET 0
#define BIND 1
#define LISTEN 2
#define ACCEPT 3
#define CONNECT 4
#define GAI 5

/* TODO [Next Semester] - Update hooks for bare linux calles to include address arguments for more in depth testing */

int sans_connect(const char*, int, int);
int sans_accept(const char*, int, int);
int sans_disconnect(int);

static tests_t tests[] = {
  {
    .category = "General",
    .prompts = {
      "Program Compiles",
      "Program correctly handles non-TCP protocols",
    }
  },
  {
    .category = "Socket",
    .prompts = {
      "Socket called",
      "Correct socket arguments",
      "Program correctly handles socket failure"
    }
  },
  {
    .category = "Bind",
    .prompts = {
      "Bind called",
      "Correct bind arguments",
      "Program correctly handles bind failure"
    }
  },
  {
    .category = "Listen",
    .prompts = {
      "Listen called",
      "Correct listen arguments",
      "Program correctly handles listen failure"
    }
  },
  {
    .category = "Accept",
    .prompts = {
      "Accept called",
      "Correct accept arguments",
    }
  },
  {
    .category = "Connect",
    .prompts = {
      "Connect called",
      "Correct connect arguments",
      "Program correctly handles connect failure"
    }
  },
  {
    .category = "Disconnect",
    .prompts = {
      "Disconnect called",
      "Correct disconnect arguments"
    }
  },
  {
    .category = "getAddrInfo",
    .prompts = {
      "getaddrinfo called",
      "Correct getaddrinfo arguments",
      "Struct freed after use"
    }
  }
};

static unsigned int call_counts[6];
static int phase = 0;

static void reset_calls(void) {
  for (int i=0; i<6; i++) {
    call_counts[i] = 0;
  }
}

static void rand_string(char* buf, int len) {
  for (int i=0; i<len; i++) {
    buf[i] = (rand() % 24) + 'a';
  }
}

static int pre_no_socket(int* result, void* args) {
  assert(0, tests[0].results[1], "FAIL - Socket called when non-TCP protocol was requested");
  *result = -1;
  return 1;
}

static int pre_no_bind(int* result, void* args) {
  assert(0, tests[0].results[1], "FAIL - Bind called when non-TCP protocol was requested");
  *result = -1;
  return 1;
}

static int pre_no_listen(int* result, void* args) {
  assert(0, tests[0].results[1], "FAIL - Listen called when non-TCP protocol was requested");
  *result = -1;
  return 1;
}

static int pre_no_accept(int* result, void* args) {
  assert(0, tests[0].results[1], "FAIL - Accept called when non-TCP protocol was requested");
  *result = -1;
  return 1;
}

static int pre_no_connect(int* result, void* args) {
  assert(0, tests[0].results[1], "FAIL - Connect called when non-TCP protocol was requested");
  *result = -1;
  return 1;
}

static int pre_no_getaddrinfo(int* result, void* args) {
  assert(0, tests[0].results[1], "FAIL - getaddrinfo called when non-TCP protocol was requested");
  *result = -1;
  return 1;
}


#define TEST_PROTO 1234
#define TEST_TYPE 5843
#define TEST_FAMILY 582
#define TEST_ALT_SOCKET 5

static char host[17];
static int port;
static int t_sock;

static struct sockaddr_in addr_in;

int __real_socket(int, int, int);
static int pre_success_socket(int* result, sockcall_t* args) {
  assert(args->protocol == TEST_PROTO, tests[1].results[1], "FAIL - socket protocol was not derived from getaddrinfo");
  assert(args->type == TEST_TYPE, tests[1].results[1], "FAIL - socket type was not derived from getaddrinfo");
  assert(args->domain == TEST_FAMILY, tests[1].results[1], "FAIL - socket family was not derived from getaddrinfo");

  if (phase == 0) {
    assert(call_counts[BIND] == 0, tests[1].results[0], "FAIL - Socket was called out of order");
    assert(call_counts[LISTEN] == 0, tests[1].results[0], "FAIL - Socket was called out of order");
    assert(call_counts[ACCEPT] == 0, tests[1].results[0], "FAIL - Socket was called out of order");
    assert(call_counts[CONNECT] == 0, tests[1].results[0], "FAIL - Socket was called out of order");
    assert(call_counts[GAI] == 1, tests[1].results[0], "FAIL - Socket was called out of order");
  }
  
  call_counts[SOCKET] += 1;
  *result = t_sock = __real_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  return 1;
}

static int pre_success_bind(int* result, int* sock) {
  assert(*sock == t_sock, tests[2].results[1], "FAIL - Bind not called on correct socket");

  if (phase == 0) {
    assert(call_counts[SOCKET] == 1, tests[2].results[0], "FAIL - Bind was called out of order");
    assert(call_counts[ACCEPT] == 0, tests[2].results[0], "FAIL - Bind was called out of order");
    assert(call_counts[CONNECT] == 0, tests[2].results[0], "FAIL - Bind was called out of order");
    assert(call_counts[GAI] == 1, tests[2].results[0], "FAIL - Bind was called out of order");
    assert(call_counts[BIND] == 0, tests[2].results[0], "FAIL - Bind called multiple times on socket success");
  }
  
  call_counts[BIND] += 1;
  *result = 0;
  return 1;
}

static int pre_success_listen(int* result, int* sock) {
  assert(*sock == t_sock, tests[3].results[1], "FAIL - Listen not called on correct socket");

  if (phase == 0) {
    assert(call_counts[SOCKET] == 1, tests[3].results[0], "FAIL - Listen was called out of order");
    assert(call_counts[ACCEPT] == 0, tests[3].results[0], "FAIL - Listen was called out of order");
    assert(call_counts[CONNECT] == 0, tests[3].results[0], "FAIL - Listen was called out of order");
    assert(call_counts[GAI] == 1, tests[3].results[0], "FAIL - Listen was called out of order");
    assert(call_counts[LISTEN] == 0, tests[3].results[0], "FAIL - Listen called multiple times on socket success");
  }

  call_counts[LISTEN] += 1;
  *result = 0;
  return 1;
}

static int pre_success_accept(int* result, int* sock) {
  assert(*sock == t_sock, tests[4].results[1], "FAIL - Accept not called on correct socket");

  if (phase == 0) {
    assert(call_counts[SOCKET] == 1, tests[4].results[0], "FAIL - Accept was called out of order");
    assert(call_counts[BIND] == 1, tests[4].results[0], "FAIL - Accept was called out of order");
    assert(call_counts[LISTEN] == 1, tests[4].results[0], "FAIL - Accept was called out of order");
    assert(call_counts[GAI] == 1, tests[4].results[0], "FAIL - Accept was called out of order");
    assert(call_counts[CONNECT] == 0, tests[4].results[0], "FAIL - Accept was called out of order");
    assert(call_counts[ACCEPT] == 0, tests[4].results[0], "FAIL - Accept called multiple times on socket success");
  }
  
  call_counts[ACCEPT] += 1;
  *result = TEST_ALT_SOCKET;
  return 1;
}

static int pre_success_connect(int* result, int* sock) {
  assert(*sock == t_sock, tests[5].results[1], "FAIL - Connect not called on correct socket");

  if (phase == 0) {
    assert(call_counts[SOCKET] == 1, tests[5].results[0], "FAIL - Connect was called out of order");
    assert(call_counts[BIND] == 0, tests[5].results[0], "FAIL - Connect was called out of order");
    assert(call_counts[LISTEN] == 0, tests[5].results[0], "FAIL - Connect was called out of order");
    assert(call_counts[GAI] == 1, tests[5].results[0], "FAIL - Connect was called out of order");
    assert(call_counts[CONNECT] == 0, tests[5].results[0], "FAIL - Connect called multiple times on socket success");
  }
  
  call_counts[CONNECT] += 1;
  *result = 0;
  return 1;
}

static struct addrinfo gai_results[2];

static int pre_getaddrinfo(int* result, gai_t* args) {
  char sport[12];
  snprintf(sport, 11, "%d", port);
  
  assert(args->node == host, tests[7].results[1], "FAIL - Host argument does not match expected value");
  assert(strcmp(args->service, sport) == 0, tests[7].results[1], "FAIL - Port argument does not match expected value");
  assert(args->hints->ai_socktype == SOCK_STREAM, tests[7].results[1], "FAIL - Requested socket type was not TCP");
  assert(args->res != 0x0, tests[7].results[1], "FAIL - Function result argument was NULL");

  if (phase == 0) {
    assert(call_counts[SOCKET] == 0, tests[7].results[0], "FAIL - getaddrinfo was called out of order");
    assert(call_counts[BIND] == 0, tests[7].results[0], "FAIL - getaddrinfo was called out of order");
    assert(call_counts[LISTEN] == 0, tests[7].results[0], "FAIL - getaddrinfo was called out of order");
    assert(call_counts[ACCEPT] == 0, tests[7].results[0], "FAIL - getaddrinfo was called out of order");
    assert(call_counts[CONNECT] == 0, tests[7].results[0], "FAIL - getaddrinfo was called out of order");
    assert(call_counts[GAI] == 0, tests[2].results[0], "FAIL - getaddrinfo called multiple times");
  }
  
  addr_in.sin_family = AF_INET;
  addr_in.sin_port = port;
  inet_aton("127.0.0.1", &(addr_in.sin_addr));

  gai_results[0].ai_family = TEST_FAMILY;
  gai_results[0].ai_socktype = TEST_TYPE;
  gai_results[0].ai_protocol = TEST_PROTO;
  gai_results[0].ai_addr = (struct sockaddr*)&addr_in;
  gai_results[0].ai_addrlen = sizeof(addr_in);
  gai_results[0].ai_next = &(gai_results[1]);
  gai_results[1].ai_family = TEST_FAMILY;
  gai_results[1].ai_socktype = TEST_TYPE;
  gai_results[1].ai_protocol = TEST_PROTO;
  gai_results[1].ai_addr = (struct sockaddr*)&addr_in;
  gai_results[1].ai_addrlen = sizeof(addr_in);
  gai_results[1].ai_next = NULL;

  if (args->res != 0x0) {
    *args->res = &(gai_results[0]);
  }
  
  call_counts[GAI] += 1;
  *result = 0;
  return 1;
}

static int pre_freeaddrinfo(int* result, struct addrinfo* ai) {
  assert(1, tests[7].results[2], "");
  return -1;
}


static int pre_fail_socket(int* result, sockcall_t* args) {
  call_counts[SOCKET] += 1;
  if (call_counts[SOCKET] == 1)
    *result = -1;
  else
    *result = t_sock = __real_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  return 1;
}

static int pre_fail_bind(int* result, int* sock) {
  call_counts[BIND] += 1;
  *result = (call_counts[BIND] == 2 ? 0 : -1);
  return 1;
}

static int pre_fail_listen(int* result, int* sock) {
  call_counts[LISTEN] += 1;
  *result = (call_counts[LISTEN] == 2 ? 0 : -1);
  return 1;
}

static int pre_fail_connect(int* result, int* sock) {
  call_counts[CONNECT] += 1;
  *result = (call_counts[CONNECT] == 2 ? 0 : -1);
  return 1;
}

static int pre_accept_close(int* result, int* fd) {
  assert(call_counts[BIND] > 0 && call_counts[LISTEN] > 0, tests[6].results[0], "FAIL - close called out of order");
  assert(*fd == t_sock, tests[6].results[1], "FAIL - Incorrect sock value passed to close");
  return 1;
}

static int pre_connect_close(int* result, int* fd) {
  assert(call_counts[CONNECT] > 0, tests[6].results[0], "FAIL - close called out of order");
  assert(*fd == t_sock, tests[6].results[1], "FAIL - Incorrect sock value passed to close");
  return 1;
}

void t__p4_tests(void) {
  host[16] = '\0';
  srand(time(NULL));
  rand_string(host, 16);
  port = (rand() % 1000) + 1000;
  
  s__initialize_tests(tests, 8);

  assert(1, tests[0].results[0], "");
  assert(1, tests[0].results[1], "");
  
  s__analytics[SOCKET_REF].precall  = (int (*)(int*, void*))pre_no_socket;
  s__analytics[BIND_REF].precall    = (int (*)(int*, void*))pre_no_bind;
  s__analytics[LISTEN_REF].precall  = (int (*)(int*, void*))pre_no_listen;
  s__analytics[ACCEPT_REF].precall  = (int (*)(int*, void*))pre_no_accept;
  s__analytics[CONNECT_REF].precall = (int (*)(int*, void*))pre_no_connect;
  s__analytics[GAI_REF].precall     = (int (*)(int*, void*))pre_no_getaddrinfo;
  s__analytics[GAIFREE_REF].precall = (int (*)(int*, void*))pre_freeaddrinfo;
  
  sans_connect(host, port, 500);
  sans_accept(host, port, 500);

  s__analytics[GAI_REF].precall     = (int (*)(int*, void*))pre_getaddrinfo;
  
  s__analytics[SOCKET_REF].precall  = (int (*)(int*, void*))pre_success_socket;
  s__analytics[BIND_REF].precall    = (int (*)(int*, void*))pre_success_bind;
  s__analytics[LISTEN_REF].precall  = (int (*)(int*, void*))pre_success_listen;
  s__analytics[ACCEPT_REF].precall  = (int (*)(int*, void*))pre_success_accept;
  s__analytics[CONNECT_REF].precall = (int (*)(int*, void*))pre_success_connect;

  reset_calls();
  sans_connect(host, port, IPPROTO_TCP);

  reset_calls();
  sans_accept(host, port, IPPROTO_TCP);

  phase = 1;
  
  s__analytics[CONNECT_REF].precall = (int (*)(int*, void*))pre_fail_connect;
  
  reset_calls();
  sans_connect(host, port, IPPROTO_TCP);

  assert(call_counts[SOCKET] > 1, tests[1].results[2], "FAIL - Socket was called too few times on connect failure");
  assert(call_counts[SOCKET] < 3, tests[1].results[2], "FAIL - Socket was called too many times on connect failure");
  assert(call_counts[CONNECT] > 1, tests[5].results[2], "FAIL - Connect was called too few times on connect failure");
  assert(call_counts[CONNECT] < 3, tests[5].results[2], "FAIL - Connect was called too main times on connect failure");

  s__analytics[CONNECT_REF].precall = (int (*)(int*, void*))pre_success_connect;
  s__analytics[BIND_REF].precall    = (int (*)(int*, void*))pre_fail_bind;
  
  reset_calls();
  sans_accept(host, port, IPPROTO_TCP);

  assert(call_counts[SOCKET] > 1, tests[1].results[2],  "FAIL - Socket was called too few times on accept failure");
  assert(call_counts[SOCKET] < 3, tests[1].results[2], "FAIL - Socket was called too many times on accept failure");
  assert(call_counts[BIND] > 1, tests[2].results[2],      "FAIL - Bind was called too few times on accept failure");
  assert(call_counts[BIND] < 3, tests[2].results[2],     "FAIL - Bind was called too main times on accept failure");  
  
  s__analytics[BIND_REF].precall = (int (*)(int*, void*))pre_success_bind;
  s__analytics[LISTEN_REF].precall  = (int (*)(int*, void*))pre_fail_listen;
  
  reset_calls();
  sans_accept(host, port, IPPROTO_TCP);

  assert(call_counts[SOCKET] > 1, tests[1].results[2],   "FAIL - Socket was called too few times on listen failure");
  assert(call_counts[SOCKET] < 3, tests[1].results[2],  "FAIL - Socket was called too many times on listen failure");
  assert(call_counts[LISTEN] > 1, tests[3].results[2],   "FAIL - Listen was called too few times on listen failure");
  assert(call_counts[LISTEN] < 3, tests[3].results[2],  "FAIL - Listen was called too main times on listen failure");

  s__analytics[LISTEN_REF].precall = (int (*)(int*, void*))pre_success_listen;
  s__analytics[SOCKET_REF].precall = (int (*)(int*, void*))pre_fail_socket;
  
  reset_calls();
  t_sock = sans_connect(host, port, IPPROTO_TCP);

  s__analytics[CLOSE_REF].precall   = (int (*)(int*, void*))pre_connect_close;
  sans_disconnect(t_sock);

  assert(call_counts[SOCKET] > 1, tests[1].results[2], "FAIL - Socket was called too few times on client socket failure");
  assert(call_counts[SOCKET] < 3, tests[1].results[2], "FAIL - Socket was called too many times on client socket failure");

  reset_calls();
  t_sock = sans_accept(host, port, IPPROTO_TCP);
  
  s__analytics[CLOSE_REF].precall   = (int (*)(int*, void*))pre_accept_close;
  sans_disconnect(t_sock);

  assert(call_counts[SOCKET] > 1, tests[1].results[2],   "FAIL - Socket was called too few times on server socket failure");
  assert(call_counts[SOCKET] < 3, tests[1].results[2],  "FAIL - Socket was called too many times on server socket failure");

  assert(strcmp(tests[6].results[0], "UNTESTED") != 0, tests[6].results[0], "FAIL - Disconnect not called");
  assert(strcmp(tests[6].results[1], "UNTESTED") != 0, tests[6].results[1], "FAIL - Incorrect socket passed to disconnect");
  
  assert(strcmp(tests[1].results[0], "UNTESTED") != 0, tests[1].results[0], "FAIL - No calls made to socket");
  assert(strcmp(tests[2].results[0], "UNTESTED") != 0, tests[2].results[0], "FAIL - No calls made to bind");
  assert(strcmp(tests[3].results[0], "UNTESTED") != 0, tests[3].results[0], "FAIL - No calls made to listen");
  assert(strcmp(tests[4].results[0], "UNTESTED") != 0, tests[4].results[0], "FAIL - No calls made to accept");
  assert(strcmp(tests[5].results[0], "UNTESTED") != 0, tests[5].results[0], "FAIL - No calls made to connect");
  assert(strcmp(tests[7].results[0], "UNTESTED") != 0, tests[7].results[0], "FAIL - No calls made to getaddrinfo");
  assert(strcmp(tests[7].results[2], "UNTESTED") != 0, tests[7].results[2], "FAIL - No calls made to freeaddrinfo");
}
