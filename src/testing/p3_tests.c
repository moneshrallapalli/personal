#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "testing.h"

#define MSG_NONE 0
#define MSG_HELO 1 << 0
#define MSG_MAIL 1 << 1
#define MSG_RCPT 1 << 2
#define MSG_DATA 1 << 3
#define MSG_DONE 1 << 4
#define MSG_QUIT 1 << 5
#define ST_NONE 0
#define ST_HELO 1
#define ST_MAIL 2
#define ST_RCPT 3
#define ST_DATA 4
#define ST_DONE 5
#define ST_QUIT 6

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
      "Program connects to server",
      "Program reads responses from server",
      "Program disconnects from server",
    }
  },
  {
    .category = "Protocol",
    .prompts = {
      "Program completes SMTP Handshake",
      "Program sets from address",
      "Program sets to address",
      "Program starts message body",
      "Program terminates message body",
      "Program ends SMTP exchange"
    }
  },
  {
    .category = "Integration",
    .prompts = {
      "Email delivered to server",
    }
  }
};

extern char* s__testdir;
static int file_len = 0;
extern int port;
extern char host[64];

extern int smtp_agent(const char*, int);

static void rand_string(char* buf, int len) {
  for (int i=0; i<len; i++) {
    buf[i] = (rand() % 24) + 'a';
  }
}


static int cmd_recv = MSG_NONE, last_msg;
static char* expected = "This is an email body\r\n.\r\n";
static int body_idx = 0;
static int in_exchange = 1;
static int pre_sans_send(int* result, arg3_t* args) {
  assert(args->socket == 5, tests[1].results[1], "FAIL - Socket was not passed to send");
  assert(args->buf != NULL, tests[1].results[1], "FAIL - Buffer did not point to valid memory");
  assert(args->len <= 1024, tests[1].results[1], "FAIL - Buffer was too large");
  assert(in_exchange == 0, tests[1].results[1], "FAIL - Sent packet while there was a pending response from server");
  if (strncmp(args->buf, "HELO", 4) == 0) {
    char state = 0;
    assert(cmd_recv == MSG_NONE, tests[2].results[0], "FAIL - HELO sent out of order");
    assert(args->len > 6, tests[2].results[0], "FAIL - Handshake did not include a origin server as part of HELO");
    for (int i=0; i < args->len; i++) 
      if (state == 0 && args->buf[i] == '\r' && args->buf[i+1] == '\n') state = 1;
    assert(state == 1, tests[2].results[0], "FAIL - Handshake not properly formatted");
    in_exchange = 1;
    cmd_recv |= MSG_HELO;
    last_msg = ST_HELO;
  }
  else if (strncmp(args->buf, "MAIL FROM:", 10) == 0) {
    assert(cmd_recv & MSG_HELO, tests[2].results[1], "FAIL - Message recieved before handshake");
    assert(!(cmd_recv & MSG_DATA), tests[2].results[1], "FAIL - Message recieved after email body start");
    assert(strncmp(args->buf, "MAIL FROM:email@example.com\r\n", 29) == 0 ||
	   strncmp(args->buf, "MAIL FROM: email@example.com\r\n", 30) == 0 ||
	   strncmp(args->buf, "MAIL FROM:<email@example.com>\r\n", 31) == 0 ||
	   strncmp(args->buf, "MAIL FROM: <email@example.com>\r\n", 32) == 0, tests[2].results[1], "FAIL - Message improperly formatted");
    in_exchange = 1;
    cmd_recv |= MSG_MAIL;
    last_msg = ST_MAIL;
  }
  else if (strncmp(args->buf, "RCPT TO:", 8) == 0) {
    assert(cmd_recv & MSG_HELO, tests[2].results[2], "FAIL - Message recieved before handshake");
    assert(!(cmd_recv & MSG_DATA), tests[2].results[2], "FAIL - Message recieved after email body start");
    assert(strncmp(args->buf, "RCPT TO:email@example.com\r\n", 27) == 0 ||
	   strncmp(args->buf, "RCPT TO: email@example.com\r\n", 28) == 0 ||
	   strncmp(args->buf, "RCPT TO:<email@example.com>\r\n", 29) == 0 ||
	   strncmp(args->buf, "RCPT TO: <email@example.com>\r\n", 30) == 0, tests[2].results[2], "FAIL - Message improperly formatted");
    in_exchange = 1;
    cmd_recv |= MSG_RCPT;
    last_msg = ST_RCPT;
  }
  else if (strncmp(args->buf, "DATA", 4) == 0) {
    assert(cmd_recv & MSG_HELO, tests[2].results[3], "FAIL - Body started before handshake");
    assert(cmd_recv & MSG_MAIL, tests[2].results[3], "FAIL - Body started before MAIL command");
    assert(cmd_recv & MSG_RCPT, tests[2].results[3], "FAIL - Body started before RCPT command");
    assert(!(cmd_recv & MSG_DATA), tests[2].results[3], "FAIL - Body resent after body start");
    assert(strncmp(args->buf, "DATA\r\n", 6) == 0, tests[2].results[3], "FAIL - Data command improperly formatted");
    in_exchange = 1;
    cmd_recv |= MSG_DATA;
    last_msg = ST_DATA;
  }
  else if (strncmp(args->buf, "QUIT", 4) == 0) {
    assert(cmd_recv == (MSG_HELO | MSG_MAIL | MSG_RCPT | MSG_DATA | MSG_DONE),
	   tests[2].results[5], "FAIL - Quit message sent before message completion");
    assert(strncmp(args->buf, "QUIT\r\n", 6) == 0, tests[2].results[5], "FAIL - Quit command improperly formatted");
    in_exchange = 1;
    cmd_recv |= MSG_QUIT;
    last_msg = ST_QUIT;
  }
  else {
    assert(cmd_recv & MSG_DATA || !(cmd_recv & MSG_DONE), tests[2].results[3], "FAIL - Non-protocol data appeared outside of DATA segment");
    for (int i=0; i<args->len; i++) {
      if (body_idx < 21) {
	assert(args->buf[i] == expected[body_idx], tests[2].results[4], "FAIL - Message body does not match file data");
      }
      else {
	assert(args->buf[i] == expected[body_idx], tests[2].results[4], "FAIL - Message termintation did not match expected string");
	in_exchange = 1;
	cmd_recv |= MSG_DONE;
	last_msg = ST_DONE;
      }
      body_idx += 1;
    }
    assert(body_idx <= strlen(expected), tests[2].results[4], "FAIL - Message body contains extra unexpected bytes");
  }
  return 0;
}

static int pre_sans_recv(int* result, arg3_t* args) {
  assert(args->socket == 5, tests[1].results[1], "FAIL - Socket was not passed to recv");
  assert(args->buf != NULL, tests[1].results[1], "FAIL - Buffer did not point to valid memory");
  assert(args->len <= 1024, tests[1].results[1], "FAIL - Buffer length was too large");
  return 0;
}

static int pre_connect(int* result, arg3_conn_t* args) {
  *result = 5;
  assert(args->port == port, tests[1].results[0], "FAIL - Port does not match the command line argument");
  assert(strcmp(args->addr, host) == 0, tests[1].results[0], "FAIL - Address does not match the command line argument");
  return -1;
}

static int pre_disconnect(int* result, arg1_t* args) {
  *result = 0;
  assert(args->socket == 5, tests[1].results[2], "FAIL - Socket was not passed to disconnect");
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
  "220 Automated testing script\r\n",
  "250 tester\r\n",
  "250 OK\r\n",
  "250 OK\r\n",
  "354 End data with <CR><LF>.<CR><LF>\r\n",
  "250 OK\r\n",
  "221 Bye\r\n"
};
static int pre_recvfrom(int* result, arg6_t* args) {
  *result = strlen(static_data[packet_in_num]);
  assert(in_exchange, tests[1].results[1], "FAIL - Awaiting response after sending packet that does not expect a response");
  in_exchange = 0;
  if (packet_in_num < 7) {
    if (cmd_recv == MSG_NONE)
      assert(packet_in_num == 0, tests[1].results[1], "FAIL - Did not recieve connection status from server");
    strncpy((char*)args->buf, static_data[last_msg], args->len);
  }
  else {
    assert(0, tests[1].results[1], "FAIL - Extra recv calls made beyond end of exchange");
    while (1);
  }
  return -1;
}

void t__p3_tests(void) {
  FILE* fp;
  char out[1024], err[1024];
  srand(time(NULL));
  rand_string(host, 16);
  port = (rand() % 8000) + 1024;
  s__initialize_tests(tests, 4);
  
  s__analytics[S_SEND_PKT_REF].precall   = (int (*)(int*, void*))pre_sans_send;
  s__analytics[S_RECV_PKT_REF].precall   = (int (*)(int*, void*))pre_sans_recv;
  s__analytics[S_CONNECT_REF].precall     = (int (*)(int*, void*))pre_connect;
  s__analytics[S_DISCONNECT_REF].precall = (int (*)(int*, void*))pre_disconnect;
  s__analytics[RECVFROM_REF].precall     = (int (*)(int*, void*))pre_recvfrom;

  assert(1, tests[0].results[0], "");

  if (s__testdir == NULL) {
    fp = fopen(".test.smtp", "w");
    fwrite("This is an email body", sizeof(char), 21, fp);
    file_len = 14;
    fclose(fp);
    s__analytics[SENDTO_REF].precall       = (int (*)(int*, void*))pre_sendto_static;
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
#ifdef HEADLESS
    s__dump_stdout(out, err);
#else
    s__dump_stdout();
#endif
  }

  char* text = "email@example.com\n.test.smtp\n";
  s__spoof_stdin(text, strlen(text));

  smtp_agent(host, port);

  assert(cmd_recv & MSG_HELO, tests[2].results[0], "FAIL - Handshake was not sent");
  assert(cmd_recv & MSG_MAIL, tests[2].results[1], "FAIL - Message origin not sent");
  assert(cmd_recv & MSG_RCPT, tests[2].results[2], "FAIL - Message destination not sent");
  assert(cmd_recv & MSG_DATA, tests[2].results[3], "FAIL - Data header not sent");
  assert(cmd_recv & MSG_DONE, tests[2].results[4], "FAIL - Message termination never sent");
  assert(cmd_recv & MSG_QUIT, tests[2].results[5], "FAIL - Quit request never sent");
  assert(strcmp(tests[1].results[1], "UNTESTED") != 0, tests[1].results[1], "FAIL - No calls made to sans_send or sans_recv");
  assert(strcmp(tests[1].results[0], "UNTESTED") != 0, tests[1].results[0], "FAIL - No calls made to sans_connect");
  assert(strcmp(tests[1].results[2], "UNTESTED") != 0, tests[1].results[2], "FAIL - No calls made to sans_disconnect");

  for (int i=0; i<6; i++)
    assert(strcmp(tests[2].results[i], "UNTESTED") != 0, tests[2].results[i], "FAIL - No message matching protocol, could not perform test");
  
  assert(in_exchange == 0, tests[1].results[1], "FAIL - Did not read response from server for each request");
}
