#define IPPROTO_RUDP 63

int http_client(const char* host, int port);
int http_server(const char* iface, int port);
int smtp_agent(const char* host, int port);
int tcp_proxy(const char* host, int port);

int sans_connect(const char* addr, int port, int protocol);
int sans_accept(const char* addr, int port, int protocol);
int sans_send_data(int socket, const char* buf, int len);
int sans_send_pkt(int socket, const char* buf, int len);
int sans_recv_data(int socket, char* buf, int len);
int sans_recv_pkt(int socket, char* buf, int len);
int sans_disconnect(int socket);
void* rudp_backend(void* unused);
