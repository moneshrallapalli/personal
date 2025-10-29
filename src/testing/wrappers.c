//#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include "testing.h"
#include "complete.h"

#define precall(r, f, a) (s__analytics[f].precall && s__analytics[f].precall(&r, &(a)) != 0)
#define postcall(r, f) if (s__analytics[f].postcall) s__analytics[f].postcall(&r)

/* Analytics info segemnt */
analytic_t s__analytics[S_MAX_REF] = { 0 };

static void timeout_handler(int signal) {
  for (int i=0; i<s__ncategories; i++) {
    for (int j=0; j<s__tests[i].ntests; j++) {
      if (strcmp(s__tests[i].results[j], "UNTESTED") == 0)
	strncpy(s__tests[i].results[j], "FAIL - Program exceeded maximum runtime [Tester timed out]", MAX_RESULT_LEN - 1);
    }
  }
  s__print_results();
  exit(-1);
}

void s__reset_analytics(void) {
  for (int i=0; i<14; i++)
    s__analytics[i].numcalls = 0;
}

int __wrap_getaddrinfo(const char* node, const char* service, const struct addrinfo* hints, struct addrinfo** res) {
  int result;
  if (precall(result, GAI_REF, ((gai_t){.node=node, .service=service, .hints=hints, .res=res})))
    return result;

  int __real_getaddrinfo(const char*, const char*, const struct addrinfo*, struct addrinfo**);
  result = __real_getaddrinfo(node, service, hints, res);
  postcall(result, GAI_REF);
  return result;
}

void __wrap_freeaddrinfo(struct addrinfo* ai) {
  int result;
  if (precall(result, GAIFREE_REF, ai))
    return;

  int __real_freeaddrinfo(struct addrinfo*);
  __real_freeaddrinfo(ai);
  postcall(result, GAIFREE_REF);
}

int __wrap_socket(int domain, int type, int protocol) {
  int result;
  if (precall(result, SOCKET_REF, ((sockcall_t){.domain=domain, .type=type, .protocol=protocol})))
    return result;
  
  int __real_socket(int, int, int);
  result = __real_socket(domain, type, protocol);
  postcall(result, SOCKET_REF);
  return result;
}

int __wrap_connect(int sock, const struct sockaddr* addr, socklen_t addrlen) {
  int result;
  if (precall(result, CONNECT_REF, sock))
    return result;
  
  int __real_connect(int, const struct sockaddr*, socklen_t);
  result = __real_connect(sock, addr, addrlen);
  postcall(result, CONNECT_REF);
  return result;
}

int __wrap_bind(int sock, const struct sockaddr* addr, socklen_t addrlen) {
  int result;
  if (precall(result, BIND_REF, sock))
    return result;

  int __real_bind(int, const struct sockaddr*, socklen_t);
  result = __real_bind(sock, addr, addrlen);
  postcall(result, CONNECT_REF);
  return result;
}

int __wrap_listen(int sock, int backlog) {
  int result;
  if (precall(result, LISTEN_REF, sock))
    return result;

  int __real_listen(int, int);
  result = __real_listen(sock, backlog);
  postcall(result, LISTEN_REF);
  return result;
}

int __wrap_accept(int sock, struct sockaddr* addr, socklen_t* addrlen) {
  int result;
  if (precall(result, ACCEPT_REF, sock))
    return result;

  int __real_accept(int, struct sockaddr*, socklen_t*);
  result = __real_accept(sock, addr, addrlen);
  postcall(result, ACCEPT_REF);
  return result;
}

ssize_t __wrap_send(int socket, const void* buf, size_t len, int flags) {
  int result;
  if (precall(result, SEND_REF, ((arg4_t){.socket=socket, .buf=buf, .len=len, .flags=flags})))
    return result;

  ssize_t __real_send(int, const void*, size_t, int);
  result = __real_send(socket, buf, len, flags);
  postcall(result, SEND_REF);
  return result;
}

ssize_t __wrap_sendto(int socket, const void* buf, size_t len, int flags, struct sockaddr* dst, socklen_t addrlen) {
  int result;
  if (precall(result, SENDTO_REF, ((arg6_t){.socket=socket, .buf=buf, .len=len, .flags=flags, .dst=dst, .addrlen=&addrlen})))
    return result;

  ssize_t __real_sendto(int, const void*, size_t, int, const struct sockaddr*, socklen_t);
  result = __real_sendto(socket, buf, len, flags, dst, addrlen);
  postcall(result, SENDTO_REF);
  return result;
}

ssize_t __wrap_recv(int socket, void* buf, size_t len, int flags) {
  int result;
  if (precall(result, RECV_REF, ((arg4_t){.socket=socket, .buf=buf, .len=len, .flags=flags})))
    return result;

  ssize_t __real_recv(int, void*, size_t, int);
  result = __real_recv(socket, buf, len, flags);
  postcall(result, RECV_REF);
  return result;
}

ssize_t __wrap_recvfrom(int socket, void* buf, size_t len, int flags, struct sockaddr* src, socklen_t* addrlen) {
  int result;
  if (precall(result, RECVFROM_REF, ((arg6_t){.socket=socket, .buf=buf, .len=len, .flags=flags, .dst=src, .addrlen=addrlen})))
    return result; 

  ssize_t __real_recvfrom(int, void*, size_t, int, struct sockaddr*, socklen_t*);
  result = __real_recvfrom(socket, buf, len, flags, src, addrlen);
  postcall(result, RECVFROM_REF);
  return result;
}

int __wrap_http_client(const char* host, int port) {
  int result;
  if (precall(result, HTTP_CLIENT_REF, ((arg2_t){.addr=host, .port=port})))
    return result;

  int __real_http_client(const char*, int);
  result = __real_http_client(host, port);
  postcall(result, HTTP_CLIENT_REF);
  return result;
}

int __wrap_http_server(const char* host, int port) {
  int result;
  if (precall(result, HTTP_SERVER_REF, ((arg2_t){.addr=host, .port=port})))
    return result;
#if PROJECT < 2
  int __instr_http_server(const char*, int);
  result = __instr_http_server(host, port);
#else
  int __real_http_server(const char*, int);
  result = __real_http_server(host, port);
#endif
  postcall(result, HTTP_SERVER_REF);
  return result;
}

int __wrap_smtp_agent(const char* host, int port) {
  int result;
  if (precall(result, SMTP_AGENT_REF, ((arg2_t){.addr=host, .port=port})))
    return result;
#if PROJECT < 3
  int __instr_smtp_agent(const char*, int);
  result = __instr_smtp_agent(host, port);
#else
  int __real_smtp_agent(const char*, int);
  result = __real_smtp_agent(host, port);
#endif
  postcall(result, SMTP_AGENT_REF);
  return result;
}

int __wrap_tcp_proxy(const char* host, int port) {
  int result;
  if (precall(result, TCP_PROXY_REF, ((arg2_t){.addr=host, .port=port})))
    return result;
#if PROJECT < 8
  int __instr_tcp_proxy(const char*, int);
  result = __instr_tcp_proxy(host, port);
#else
  int __real_tcp_proxy(const char*, int);
  result = __real_tcp_proxy(host, port);
#endif
  postcall(result, TCP_PROXY_REF);
  return result;
}

void* __wrap_rudp_backend(void* unused) {
#if PROJECT < 6
  
#else
  int __real_rudp_backend(void*);
  __real_rudp_backend(unused);
#endif
  return NULL;
}

int __wrap_sans_connect(const char* addr, int port, int protocol) {
  int result;
  if (precall(result, S_CONNECT_REF, ((arg3_conn_t){.addr=addr, .port=port, .protocol=protocol})))
    return result;
#if PROJECT < 4
  int __instr_connect(const char*, int, int);
  result = __instr_connect(addr, port, protocol);
#else
  int __real_sans_connect(const char*, int, int);
  result = __real_sans_connect(addr, port, protocol);
#endif
  postcall(result, S_CONNECT_REF);
  return result;
}

int __wrap_sans_accept(const char* addr, int port, int protocol) {
  int result;
  if (precall(result, S_ACCEPT_REF, ((arg3_conn_t){.addr=addr, .port=port, .protocol=protocol})))
    return result;
#if PROJECT < 4
  int __instr_accept(const char*, int, int);
  result = __instr_accept(addr, port, protocol);
#else
  int __real_sans_accept(const char*, int, int);
  result = __real_sans_accept(addr, port, protocol);
#endif
  postcall(result, S_ACCEPT_REF);
  return result;
}

int __wrap_sans_send_pkt(int socket, const char* buf, int len) {
  int result;
  if (precall(result, S_SEND_PKT_REF, ((arg3_t){.socket=socket, .buf=buf, .len=len})))
    return result;
#if PROJECT < 5
  int __instr_send_pkt(int, const char*, int);
  result = __instr_send_pkt(socket, buf, len);
#else
  int __real_sans_send_pkt(int, const char*, int);
  result = __real_sans_send_pkt(socket, buf, len);
#endif
  postcall(result, S_SEND_PKT_REF);
  return result;
}

int __wrap_sans_recv_pkt(int socket, char* buf, int len) {
  int result;
  if (precall(result, S_RECV_PKT_REF, ((arg3_t){.socket=socket, .buf=buf, .len=len})))
    return result;
#if PROJECT < 5
  int __instr_recv_pkt(int, char*, int);
  result = __instr_recv_pkt(socket, buf, len);
#else
  int __real_sans_recv_pkt(int, char*, int);
  result = __real_sans_recv_pkt(socket, buf, len);
#endif
  postcall(result, S_RECV_PKT_REF);
  return result;
}

int __wrap_sans_disconnect(int socket) {
  int result;
  if (precall(result, S_DISCONNECT_REF, ((arg1_t){.socket=socket})))
    return result;
#if PROJECT < 4
  int __instr_disconnect(int socket);
  result = __instr_disconnect(socket);
#else
  int __real_sans_disconnect(int socket);
  result = __real_sans_disconnect(socket);
#endif
  postcall(result, S_DISCONNECT_REF);
  return result;
}

int __wrap_close(int fd) {
  int result;
  if (precall(result, CLOSE_REF, fd))
    return result;

  int __real_close(int);
  result = __real_close(fd);

  postcall(result, CLOSE_REF);
  return result;
}

int __wrap_main(int argc, char** argv) {
  signal(SIGALRM, timeout_handler);
  alarm(9);
  
#if PROJECT == 1
  void t__p1_tests(void);
  t__p1_tests();
#elif PROJECT == 2
  void t__p2_tests(void);
  t__p2_tests();
#elif PROJECT == 3
  void t__p3_tests(void);
  t__p3_tests();
#elif PROJECT == 4
  void t__p4_tests(void);
  t__p4_tests();
#elif PROJECT == 5
  void t__p5_tests(void);
  t__p5_tests();
#elif PROJECT == 6
  void t__p6_tests(void);
  t__p6_tests();
#elif PROJECT == 7 
  void t__p7_tests(void);
  t__p7_tests();
#elif PROJECT == 8
  void t__p8_tests(void);
  t__p8_tests();
#endif
  s__print_results();
  return 0;
}
