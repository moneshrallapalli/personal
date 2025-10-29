#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include "testing.h"

#define IPPROTO_RUDP 63

#define DAT 0
#define SYN 1
#define ACK 2
#define FIN 4

#define SYNACK 3

#define CONN 1
#define ACPT 2
#define BOTH 3

typedef struct {
  char type;
  int seqnum;
  char payload[];
} rudp_packet_t;

int sans_connect(const char*, int, int);
int sans_accept(const char*, int, int);
int sans_send_pkt(int, char*, int);

extern char* s__testdir;

static tests_t tests[] = {
  {
    .category = "General",
    .prompts = {
      "Program Compiles",
      "Functions accept RUDP Protocol type",
      "Functions does not call buildin listen",
      "Functions does not call builtin accept",
      "Functions does not call builtin connect",
      "Functions does not call builtin send",
      "Functions does not call builtin recv",
    }
  },
  {
    .category = "Connect",
    .prompts = {
      "Connect uses UDP",
      "Function sends SYN",
      "Function awaits SYNACK",
      "Function retransmits SYN on timeout",
      "Function sends ACK"
    }
  },
  {
    .category = "Accept",
    .prompts = {
      "Accept uses UDP",
      "Function awaits SYN",
      "Function discards erroneous packets",
      "Function sends SYNACK",
      "Function awaits ACK",
      "Function retransmits SYNACK on timeout"
    }
  },
  {
    .category = "State",
    .prompts = {
      "Send uses address and length from handshake",
    }
  }
};

static int side = 0;
static int pre_socket(int* result, sockcall_t* args) {
  assert(!(side & CONN) || args->protocol == IPPROTO_UDP, tests[1].results[0], "FAIL - Socket not created using the UDP protocol");
  assert(!(side & CONN) || args->type == SOCK_DGRAM, tests[1].results[0], "FAIL - Socket not created using Datagrams");
  assert(!(side & ACPT) || args->protocol == IPPROTO_UDP, tests[2].results[0], "FAIL - Socket not created using the UDP protocol");
  assert(!(side & ACPT) || args->type == SOCK_DGRAM, tests[2].results[0], "FAIL - Socket not created using Datagrams");
  return 0;
}

static int pre_listen(int* result, int* sock) {
  assert(0, tests[0].results[2], "Listen was called");
  return 0;
}

static int pre_accept(int* result, int* sock) {
  assert(0, tests[0].results[3], "Accept was called");
  return 0;
}

static int pre_connect(int* result, int* sock) {
  assert(0, tests[0].results[4], "Connect was called");
  return 0;
}

static int pre_send(int* result, int* sock) {
  assert(0, tests[0].results[5], "Send was called");
  return 0;
}

static int pre_recv(int* result, int* sock) {
  assert(0, tests[0].results[6], "Recv was called");
  return 0;
}

static int conn_stage = SYN;
static int conn_send_count = 0;
static int pre_connect_sendto(int* result, arg6_t* args) {
  rudp_packet_t* pkt = (rudp_packet_t*)args->buf;
  assert(conn_stage == SYN || conn_stage == ACK, tests[1].results[1], "FAIL - Packet sent from connect out of order");
  if (conn_stage == SYN) {
    assert(pkt->type == SYN, tests[1].results[1], "FAIL - Non-SYN packet sent by connect");
    conn_stage = SYNACK;
  }
  if (conn_stage == ACK)
    assert(pkt->type == ACK, tests[1].results[4], "FAIL - Incorrect packet type sent for final acknowledgement");
  conn_send_count += 1;
  *result = args->len;
  return 1;
}

static int buff_to_arg6(arg6_t* args, char* buff, int max) {
  int i;
  for (i=0; i<args->len && i<max; i++) {
    ((char*)args->buf)[i] = buff[i];
  }
  return i;
}

static int pre_connect_recvfrom(int* result, arg6_t* args) {
  assert(conn_stage == SYNACK, tests[1].results[2], "FAIL - Awaiting at incorrect stage in handshake");
  if (conn_send_count == 1) {
    *result = -1;
    conn_stage = SYN;
  }
  else {
    rudp_packet_t pkt = {
      .type = SYN | ACK,
      .seqnum = 0
    };
    *result = buff_to_arg6(args, (char*)&pkt, sizeof(rudp_packet_t));
    conn_stage = ACK;
  }
  return 1;
}

static int acpt_stage = SYN;
static int acpt_send_count = 0;
static int pre_accept_sendto(int* result, arg6_t* args) {
  rudp_packet_t* pkt = (rudp_packet_t*)args->buf;
  assert(acpt_stage == SYNACK, tests[2].results[3], "FAIL - Packet sent out of order in handshake");
  if (acpt_stage == SYNACK) {
    assert(pkt->type == (SYN | ACK), tests[2].results[3], "FAIL - Non-SYNACK packet sent during handshake");
    acpt_stage = ACK;
  }

  acpt_send_count += 1;
  *result = args->len;
  return 1;
}

typedef struct sockaddr_in sa_in;
static int acpt_recv_count = 0;
static int pre_accept_recvfrom(int* result, arg6_t* args) {
  sa_in addr = {
    .sin_family = AF_INET,
    .sin_port = 80,
    .sin_addr = { .s_addr = 338186360 }
  };
  memcpy((char*)args->dst, &addr, sizeof(addr));
  *args->addrlen = 16;
  rudp_packet_t pkt = {
    .seqnum = 0
  };
  if (acpt_stage == SYN) {
    assert(1, tests[2].results[1], "");
    if (acpt_recv_count == 0) {
      pkt.type = ACK;
    }
    else {
      pkt.type = SYN;
      acpt_stage = SYNACK;
    }
    *result = buff_to_arg6(args, (char*)&pkt, sizeof(rudp_packet_t));;
  }
  else if (acpt_stage == ACK) {
    assert(1, tests[2].results[4], "");
    if (acpt_recv_count < 3) {
      *result = -1;
      acpt_stage = SYNACK;
    }
    else {
      pkt.type = ACK;
      *result = buff_to_arg6(args, (char*)&pkt, sizeof(rudp_packet_t));
    }
  }
  else {
    assert(acpt_stage == SYN || acpt_stage == ACK, tests[2].results[1], "FAIL - Awaiting at incorrect stage in handshake");
    assert(acpt_stage == SYN || acpt_stage == ACK, tests[2].results[4], "FAIL - Awaiting at incorrect stage in handshake");
  }
  acpt_recv_count += 1;
  return 1;

}

static int pre_sendto(int* result, arg6_t* args) {
  sa_in* addr = (sa_in*)args->dst;
  assert(addr->sin_port == 80 && addr->sin_addr.s_addr == 338186360, tests[3].results[0], "FAIL - Address in sendto does not match remote host address");
  *result = args->len;
  return 1;
}

void t__p5_tests(void) {
  char buf[1024] = {0};
  char out[128], err[128];
  
  s__initialize_tests(tests, 4);

  if (s__testdir == NULL) {
#ifdef HEADLESS
    s__dump_stdout("test.out", "test.err");
#else
    s__dump_stdout();
#endif
  }
  else {
    snprintf(out, 1024, "%s/stdout", s__testdir);
    snprintf(err, 1024, "%s/stderr", s__testdir);
#ifdef HEADLESS
    s__dump_stdout(out, err);
#else
    s__dump_stdout();
#endif
  }

  assert(1, tests[0].results[0], "");

  s__analytics[SOCKET_REF].precall  = (int (*)(int*, void*))pre_socket;
  s__analytics[CONNECT_REF].precall  = (int (*)(int*, void*))pre_connect;
  s__analytics[LISTEN_REF].precall  = (int (*)(int*, void*))pre_listen;
  s__analytics[ACCEPT_REF].precall  = (int (*)(int*, void*))pre_accept;
  s__analytics[SEND_REF].precall  = (int (*)(int*, void*))pre_send;
  s__analytics[RECV_REF].precall  = (int (*)(int*, void*))pre_recv;
  
  s__analytics[SENDTO_REF].precall  = (int (*)(int*, void*))pre_connect_sendto;
  s__analytics[RECVFROM_REF].precall  = (int (*)(int*, void*))pre_connect_recvfrom;

  // Test sans_connect
  side = CONN;
  sans_connect("192.168.88.81", 80, IPPROTO_RUDP);
  assert(conn_send_count < 4, tests[1].results[3], "FAIL - Too many sends from connect");
  assert(conn_send_count > 2, tests[1].results[3], "FAIL - No retransmission of SYN on failure");
  
  s__analytics[SENDTO_REF].precall  = (int (*)(int*, void*))pre_accept_sendto;
  s__analytics[RECVFROM_REF].precall  = (int (*)(int*, void*))pre_accept_recvfrom;

  // Test sans_accept  
  side = ACPT;
  int sock = sans_accept("127.0.0.1", 9999, IPPROTO_RUDP);
  assert(acpt_send_count < 3, tests[2].results[5], "FAIL - Too many sends from connect");
  assert(acpt_send_count > 1, tests[2].results[5], "FAIL - No retransmission of SYN on failure");
  assert(acpt_recv_count == 4, tests[2].results[2], "FAIL - Recv called too few times after bad send");

  s__analytics[SENDTO_REF].precall  = (int (*)(int*, void*))pre_sendto;

  // Test sans_send_pkt
  side = BOTH;
  for (int i=1; i<7; i++) {
    assert(strcmp(tests[0].results[i], "UNTESTED") == 0, tests[0].results[i], "FAIL - Function was called");
  }
  
  assert(strcmp(tests[1].results[1], "UNTESTED") != 0, tests[1].results[1], "FAIL - Inconclusive test on send");
  assert(strcmp(tests[1].results[2], "UNTESTED") != 0, tests[1].results[2], "FAIL - Inconclusive test on recv");
  assert(strcmp(tests[1].results[3], "UNTESTED") != 0, tests[1].results[3], "FAIL - Inconclusive test on send");
  assert(strcmp(tests[1].results[4], "UNTESTED") != 0, tests[1].results[4], "FAIL - Inconclusive test on send");
  assert(strcmp(tests[2].results[1], "UNTESTED") != 0, tests[2].results[1], "FAIL - Inconclusive test on recv");
  assert(strcmp(tests[2].results[2], "UNTESTED") != 0, tests[2].results[2], "FAIL - Inconclusive test on recv");
  assert(strcmp(tests[2].results[3], "UNTESTED") != 0, tests[2].results[3], "FAIL - Inconclusive test on send");
  assert(strcmp(tests[2].results[1], "UNTESTED") != 0, tests[2].results[1], "FAIL - Inconclusive test on recv");
  assert(strcmp(tests[2].results[5], "UNTESTED") != 0, tests[2].results[5], "FAIL - Inconclusive test on send");

  sans_send_pkt(sock, buf, 100);

}
