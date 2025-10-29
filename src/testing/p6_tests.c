#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include "testing.h"

#define IPPROTO_RUDP 63

#define DAT 0
#define SYN 1
#define ACK 2
#define FIN 4

#define SEND 1
#define RECV 2

static int test_set;

typedef struct {
  char type;
  int seqnum;
  char payload[];
} rudp_packet_t;

int sans_connect(const char*, int, int);
int sans_accept(const char*, int, int);

int sans_send_pkt(int, char*, int);
int sans_recv_pkt(int, char*, int);

extern char* s__testdir;


static tests_t tests[] = {
  {
    .category = "General",
    .prompts = {
      "Program Compiles",
    }
  },
  {
    .category = "Send",
    .prompts = {
      "Transmitted Data Correctness",
      "Sequence number",
      "Frist Packet Timeout",
      "Later Packet Timeout",
      "Out of Order Acknowledgement"
    }
  },
  {
    .category = "Recv",
    .prompts = {
      "Recieved Data Correctness",
      "Acknowledgement Number",
      "Duplicate Sequence Number",
      "Out of Order Sequence Number"
    }
  }
};

static jmp_buf env;

static int* send_seq_p = NULL;
static int* recv_seq_p = NULL;
static int current_packet   =  0;
static int base_send_seq    =  0;
static int base_recv_seq    =  0;
static int current_send_seq = -1;
static char send_success    =  0;
static void reset_sequence(void) {
  current_send_seq = -1;
  current_packet = 0;
  send_success = 1;
  *send_seq_p = base_send_seq;
  *recv_seq_p = base_recv_seq;
}


static char* send_data[] = {
  "Test phrase 1",
  "Another phase",
  "some data generated",
  "around here there are",
  "Malifactors inter works",
  "shorter string",
  "empty packet",
  "A slightly larger string containing things"
};

static void sigsegv_handler(int sig) {
  siglongjmp(env, 1);
}

/* ---- Send Tests ---- */
static int recorded_seq[6];
static int stable_pass = 1;
static int timeout_count = 0;
static int bad_seq;
static int test_num;
static char* seq_err;
static char* data_err;
static sem_t recv_lock;

static int validate_packet(rudp_packet_t* pkt) {
  if (test_set == SEND && stable_pass == 1)
    recorded_seq[current_packet] = pkt->seqnum;
  if (send_success)
    assert(current_send_seq != pkt->seqnum, tests[test_set].results[test_num], seq_err);
  else
    assert(current_send_seq == pkt->seqnum, tests[test_set].results[test_num], seq_err);
  if (test_set == SEND) {
    assert(pkt->type == DAT, tests[test_set].results[test_num], "FAIL - Packet was not set to DAT");
    assert(strcmp(send_data[current_packet], pkt->payload) == 0, tests[test_set].results[0], data_err);
  }
  else {
    assert(pkt->type == ACK, tests[2].results[test_num], "FAIL - Packet was not set to ACK");
  }
  return pkt->seqnum;
}

static int pre_sendto_testing(int* result, arg6_t* args) {
  current_send_seq = validate_packet((rudp_packet_t*)args->buf);
  *result = args->len;
  return 1;
}

static int pre_sendto_nosend(int* result, arg6_t* args) {
  assert(0, tests[test_set].results[test_num], "FAIL - Packet resent after out-of-order acknowledgement");
  *result = args->len;
  return 1;
}

static int pre_sendto_noresend(int* result, arg6_t* args) {
  s__analytics[SENDTO_REF].precall = (int (*)(int*, void*))pre_sendto_nosend;
  current_send_seq = validate_packet((rudp_packet_t*)args->buf);
  *result = args->len;
  return 1;
}

static int pre_recvfrom_success(int* result, arg6_t* args) {
  rudp_packet_t pkt = {
    .seqnum = current_send_seq,
    .type = ACK
  };

  memcpy((char*)args->buf, &pkt, sizeof(pkt));
  send_success = 1;
  *result = sizeof(pkt);
  sem_post(&recv_lock);
  return 1;
}

static int pre_recvfrom_timeout(int* result, arg6_t* args) {
  if (timeout_count <= 1) {
    timeout_count -= 1;
    s__analytics[RECVFROM_REF].precall = (int (*)(int*, void*))pre_recvfrom_success;
    s__analytics[SENDTO_REF].precall   = (int (*)(int*, void*))pre_sendto_testing;
  }
  send_success = 0;
  errno = EWOULDBLOCK;
  *result = -1;
  return 1;
}

static int pre_recvfrom_order(int* result, arg6_t* args) {
  rudp_packet_t pkt = {
    .seqnum = bad_seq,
    .type = ACK
  };

  s__analytics[RECVFROM_REF].precall = (int (*)(int*, void*))pre_recvfrom_timeout;
  memcpy((char*)args->buf, &pkt, sizeof(pkt));
  send_success = 0;
  *result = sizeof(pkt);
  return 1;
}

static void run_send_test(int sock, int packet_count,
			  int timeout, int timeout_offset,
			  int order, int order_offset) {
  current_packet = 0;
  reset_sequence();

  for (int i=0; i<packet_count; i++) {
    s__analytics[SENDTO_REF].precall   = (int (*)(int*, void*))pre_sendto_testing;
    if ((timeout != -1) && (timeout_offset-- == 0)) {
      timeout_count = timeout;
      s__analytics[RECVFROM_REF].precall = (int (*)(int*, void*))pre_recvfrom_timeout;
    }
    else if ((order != -1) && (order_offset-- == 0)) {
      bad_seq = order;
      s__analytics[RECVFROM_REF].precall = (int (*)(int*, void*))pre_recvfrom_order;
      s__analytics[SENDTO_REF].precall   = (int (*)(int*, void*))pre_sendto_noresend;
    }

    sans_send_pkt(sock, send_data[current_packet], strlen(send_data[current_packet]) + 1);
    sem_wait(&recv_lock);
    current_packet += 1;
  }
}

/* ---------------- Recv Tests ----------------------- */
static int pre_recvfrom_testing(int* result, arg6_t* args) {
  rudp_packet_t* pkt = (rudp_packet_t*)args->buf;
  pkt->type = DAT;
  pkt->seqnum = recorded_seq[current_packet];
  send_success = 1;

  memcpy(pkt->payload, send_data[current_packet], strlen(send_data[current_packet]) + 1);
  *result = sizeof(pkt) + strlen(send_data[current_packet]) + 1;
  return 1;
}

static int pre_recvfrom_duplicate(int* result, arg6_t* args) {
  rudp_packet_t* pkt = (rudp_packet_t*)args->buf;
  pkt->type = DAT;
  pkt->seqnum = recorded_seq[current_packet - 1];
  send_success = 0;

  s__analytics[RECVFROM_REF].precall = (int (*)(int*, void*))pre_recvfrom_testing;
  
  memcpy(pkt->payload, send_data[current_packet-1], strlen(send_data[current_packet-1]) + 1);
  *result = sizeof(pkt) + strlen(send_data[current_packet-1]) + 1;
  return 1;
}

static int pre_recvfrom_order_2(int* result, arg6_t* args) {
  rudp_packet_t* pkt = (rudp_packet_t*)args->buf;
  pkt->type = DAT;
  pkt->seqnum = recorded_seq[current_packet + 1];
  send_success = 0;

  s__analytics[RECVFROM_REF].precall = (int (*)(int*, void*))pre_recvfrom_testing;

  memcpy(pkt->payload, send_data[current_packet+1], strlen(send_data[current_packet+1]) + 1);
  *result = sizeof(pkt) + strlen(send_data[current_packet-1]) + 1;
  return 1;
}

static void run_recv_test(int sock, int packet_count, int order) {
  current_packet = 0;
  reset_sequence();

  for (int i=0; i<packet_count; i++) {
    char packet[1024] = { 0 };
    s__analytics[SENDTO_REF].precall   = (int (*)(int*, void*))pre_sendto_testing;
    if (order == 1 && i == 1) {
      s__analytics[RECVFROM_REF].precall = (int (*)(int*, void*))pre_recvfrom_duplicate;
    }
    else if (order == 2 && i == 1) {
      s__analytics[RECVFROM_REF].precall = (int (*)(int*, void*))pre_recvfrom_order_2;
    }

    sans_recv_pkt(sock, packet, 1024);
    assert(strcmp(packet, send_data[current_packet]) == 0, tests[2].results[0], data_err);
    current_packet += 1;
  }
}

/* ----------------------- Pre-configuration --------------------*/

typedef struct sockaddr_in sa_in;
static sa_in addr;
static int pre_sendto_handshake(int* result, arg6_t* args) {
  memcpy(&addr, (char*)args->dst, *args->addrlen);
  *result = args->len;
  return 1;
}

static int pre_recvfrom_handshake(int* result, arg6_t* args) {
  if (args->dst != NULL)
    memcpy((char*)args->dst, &addr, sizeof(addr));
  if (args->addrlen != NULL)
    *args->addrlen = 16;
  *result = 1;
  ((char*)args->buf)[0] = SYN | ACK;
  return 1;
}

static int seqnum;
static char recv_type;
static int pre_sendto(int* result, arg6_t* args) {
  seqnum = ((rudp_packet_t*)args->buf)->seqnum;
  *result = args->len;
  return 1;
}

static int pre_recvfrom(int* result, arg6_t* args) {
  memcpy((char*)args->dst, &addr, sizeof(addr));
  *args->addrlen = 16;
  rudp_packet_t pkt = {
    .seqnum = seqnum,
    .type = recv_type
  };

  *result = 8;
  *(rudp_packet_t*)args->buf = pkt;
  return 1;
}

extern char data_start;
extern char edata;
extern char end;
static char* snapshot = NULL;
static int* find_static_address(char* snapshot) {
  long diff = 0;
  int* result = 0;
  char* data = &data_start;
  for (int i=0; i<(&end-data); i++) {
    if (snapshot[i] != data[i]) {
      if (diff == 0)
	diff = i;
    }
    else if (diff != 0) {
      char* s = data + diff, *e = data + i;
      int old = *(int*)(&snapshot[diff]);
      int new = *(int*)s;
      
      printf("Candidate sequence number address found\n");
      printf("  | Diff (%p-%p) - %ld bytes\n", s, e, e - s);
      printf("  +--- %d --> %d\n", old,  new);
      if (old == seqnum && new == seqnum + 1) {
	result = (int*)s;
      }
      diff = 0;
    }
  }
  printf("\n");
  return result;
}

static void infer_static_addresses(int sock) {
  unsigned long offset;
  char* data = &data_start;
  
  snapshot = malloc(sizeof(char) * (&end - data));

  printf("Auto-detecting sequence and acknowledgement number addresses\n");
  printf("   | Data segment (%p-%p] - %ld bytes\n", data, &edata, &edata - data);
  printf("   | BSS segment  (%p-%p] - %ld bytes\n\n", &edata, &end, &end - &edata);

  s__analytics[SENDTO_REF].precall  = (int (*)(int*, void*))pre_sendto;
  s__analytics[RECVFROM_REF].precall  = (int (*)(int*, void*))pre_recvfrom;

  {
    recv_type = ACK;
    memcpy(snapshot, data, &end - data);
    sans_send_pkt(sock, "test", 4);
    send_seq_p = find_static_address(snapshot);
    offset = (char*)send_seq_p - data;
    base_send_seq = snapshot[offset];
  }
  {
    char buf[8];
    recv_type = DAT;
    *send_seq_p = seqnum = base_send_seq;
    memcpy(snapshot, data, &end - data);
    sans_recv_pkt(sock, buf, 8);
    recv_seq_p = find_static_address(snapshot);
    offset = (char*)recv_seq_p - data;
    base_recv_seq = snapshot[offset];
  }

  printf("Base send sequence number: %d\n", base_send_seq);
  printf("Base recv sequence number: %d\n", base_recv_seq);
  *send_seq_p = base_send_seq;
  *recv_seq_p = base_recv_seq;
}

void t__p6_tests(void) {
  char out[128], err[128];

  s__initialize_tests(tests, 3);

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

  s__analytics[SENDTO_REF].precall  = (int (*)(int*, void*))pre_sendto_handshake;
  s__analytics[RECVFROM_REF].precall  = (int (*)(int*, void*))pre_recvfrom_handshake;

  { /*  Transport Driver thread  */
    pthread_t backend_thread;
    void* rudp_backend(void*);
    int result = pthread_create(&backend_thread, NULL, rudp_backend, NULL);
    if (result != 0) {
      fprintf(stderr, "Failed to create background worker thread\n");
      exit(-1);
    }
  }

  int sock = sans_connect("localhost", 80, IPPROTO_RUDP);
  
  if (sigsetjmp(env, 1) == 0) {
    signal(SIGSEGV, sigsegv_handler);
    infer_static_addresses(sock);
    signal(SIGSEGV, SIG_DFL);
  }
  else {
    assert(0, tests[0].results[0], "FAIL - Segmentation fault during setup, could not find sequence number in memory");
    signal(SIGSEGV, SIG_DFL);
    return;
  }
  /* ---- Send Tests ---- */
  test_set = SEND;
  sem_init(&recv_lock, 0, 0);
  s__analytics[SENDTO_REF].precall   = (int (*)(int*, void*))pre_sendto_testing;
  s__analytics[RECVFROM_REF].precall = (int (*)(int*, void*))pre_recvfrom_success;
  
  /* run_send_test(socket, #packets, timeout, timeout_offset, bad_seq, bad_seq_offset) */
  seq_err  = "FAIL - Sequence number did not change between sucessful sends";
  data_err = "FAIL - Data sent does not match expected payload";
  test_num = 1;
  run_send_test(sock, 6, -1, -1, -1, -1);
  stable_pass = 0;

  seq_err  = "FAIL - Sequence number was out of order when first packet times out";
  data_err = "FAIL - Wrong data was sent when first packet times out";
  test_num = 2;
  run_send_test(sock, 6, 1, 0, -1, -1);

  seq_err  = "FAIL - Sequence number was out of order when middle packet times out";
  data_err = "FAIL - Wrong data was sent when middle packet times out";
  test_num = 3;
  run_send_test(sock, 6, 1, 2, -1, -1);

  seq_err  = "FAIL - Sequence number was incorrect when duplicate ack is recieved";
  data_err = "FAIL - Wrong data was sent after a duplicate ack was recieved";
  test_num = 4;
  run_send_test(sock, 6, -1, -1, base_send_seq, 1);

  seq_err  = "FAIL - Sequence number was incorrect when an out of order ack is recieved";
  data_err = "FAIL - Wrong data was sent after an out of order ack was recieved";
  test_num = 4;
  run_send_test(sock, 6, -1, -1, base_send_seq + 3, 2);


  /* ---- Recv Tests ---- */
  test_set = RECV;
  s__analytics[RECVFROM_REF].precall = (int (*)(int*, void*))pre_recvfrom_testing;
  
  /* static void run_recv_test(int sock, int packet_count, int order); */
  seq_err  = "FAIL - Unexpected acknowledgement for packets in successful sequence";
  data_err = "FAIL - Data returned from call did not match data sent to reciever";
  test_num = 1;
  run_recv_test(sock, 6, 0);

  seq_err  = "FAIL - Unexpected acknowledgement from duplicate packet";
  data_err = "FAIL - Data returned from call was not the next sucessful packet on duplicate";
  test_num = 2;
  run_recv_test(sock, 6, 1);

  seq_err  = "FAIL - Unexpected acknowledgement from out of order packet";
  data_err = "FAIL - Data returned from call was not the next sucessful packet on out of order packet";
  test_num = 3;
  run_recv_test(sock, 6, 2);
  
}
