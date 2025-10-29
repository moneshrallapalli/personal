#include <sys/socket.h>
#define SOCKET_REF        0
#define CONNECT_REF       1
#define GAI_REF           2
#define BIND_REF          3
#define LISTEN_REF        4
#define ACCEPT_REF        5
#define SEND_REF          6
#define SENDTO_REF        7
#define RECV_REF          8
#define RECVFROM_REF      9
#define HTTP_CLIENT_REF  10
#define HTTP_SERVER_REF  11
#define SMTP_AGENT_REF   12
#define TCP_PROXY_REF    13
#define S_CONNECT_REF    14
#define S_ACCEPT_REF     15
#define S_SEND_DATA_REF  16
#define S_SEND_PKT_REF   17
#define S_RECV_DATA_REF  18
#define S_RECV_PKT_REF   19
#define S_DISCONNECT_REF 20
#define GAIFREE_REF      21
#define CLOSE_REF        22
#define S_MAX_REF        23

#define MAX_TESTS 20
#define MAX_RESULT_LEN 1024
//#define HEADLESS

int assert(int, char*, const char*);

typedef struct {
  const char* node;
  const char* service;
  const struct addrinfo* hints;
  struct addrinfo** res;
} gai_t;

typedef struct {
  int domain;
  int type;
  int protocol;
} sockcall_t;

typedef struct {
  int numcalls;
  int (*precall)(int*, void*);
  int (*postcall)(void*);
} analytic_t;

typedef struct {
  int socket;
} arg1_t;

typedef struct {
  const char* addr;
  int port;
} arg2_t;

typedef struct {
  int socket;
  const char* buf;
  int len;
} arg3_t;

typedef struct {
  const char* addr;
  int port;
  int protocol;
} arg3_conn_t;

typedef struct {
  int socket;
  const void* buf;
  size_t len;
  int flags;
} arg4_t;

typedef struct {
  int socket;
  const void* buf;
  size_t len;
  int flags;
  struct sockaddr* dst;
  socklen_t* addrlen;
} arg6_t;

typedef struct {
  const char* category;
  unsigned int ntests;
  const char* prompts[MAX_TESTS];
  char results[MAX_TESTS][MAX_RESULT_LEN];
} tests_t;

extern tests_t* s__tests;
extern analytic_t s__analytics[S_MAX_REF];
extern unsigned int s__ncategories;

void s__initialize_tests(tests_t*, int);
void s__print_results(void);
void s__spoof_stdin(char*, int);
#ifdef HEADLESS
void s__dump_stdout(const char*, const char*);
#else
void s__dump_stdout(void);
#endif
void s__restore_stdout(void);
