

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <errno.h>
#include "rudp.h"  


#ifndef RUDP_SYN
#define RUDP_SYN 1
#endif

#ifndef RUDP_ACK
#define RUDP_ACK 2
#endif

#ifndef RUDP_FIN
#define RUDP_FIN 4
#endif


int rudp_save_peer(int sock, const struct sockaddr *sa, socklen_t slen);




static int port_to_str(char out[12], int port) {
    if (port <= 0) {
        return -1;
    }
    if (port > 65535) {
        return -1;
    }

    char tmp[12];
    int i = 0;
    int p = port;

    while (p > 0 && i < 11) {
        int digit = p % 10;
        tmp[i] = (char)('0' + digit);
        i = i + 1;
        p = p / 10;
    }

    if (p > 0) {
        return -1;
    }

    if (i == 0) {
        tmp[i] = '0';
        i = i + 1;
    }

    int j = 0;
    int out_idx = 0;

    while (j < i) {
        int source_idx = i - 1 - j;
        out[out_idx] = tmp[source_idx];
        out_idx = out_idx + 1;
        j = j + 1;
    }

    out[out_idx] = '\0';
    return 0;
}

static void zero_bytes(void *p, size_t n) {
    unsigned char *b = (unsigned char*)p;
    size_t i = 0;

    while (i < n) {
        b[i] = 0;
        i = i + 1;
    }
}


static int set_recv_timeout_20ms(int sock) {
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 20000;

    int result = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    return result;
}



int sans_connect(const char *host, int port, int protocol) {
    if (host == 0) {
        return -1;
    }

    char service[12];
    int port_result = port_to_str(service, port);
    if (port_result != 0) {
        return -1;
    }

    if (protocol == IPPROTO_TCP) {
        int final_fd = -1;

        struct addrinfo hints;
        struct addrinfo *results = 0;

        zero_bytes(&hints, sizeof(hints));
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        int gai_ok = getaddrinfo(host, service, &hints, &results);
        if (gai_ok != 0 || results == 0) {
            return -1;
        }

        struct addrinfo *p = results;
        while (p != 0) {
            int fd_try = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (fd_try >= 0) {
                int ok_conn = connect(fd_try, p->ai_addr, p->ai_addrlen);
                if (ok_conn == 0) {
                    final_fd = fd_try;
                    p = 0;
                    break;
                } else {
                    close(fd_try);
                }
            }
            if (p != 0) {
                p = p->ai_next;
            }
        }

        freeaddrinfo(results);
        return final_fd;
    }

 
#ifdef IPPROTO_RUDP
    if (protocol == IPPROTO_RUDP)
#else
    if (protocol != IPPROTO_TCP)
#endif
    {
        int final_fd = -1;

        struct addrinfo hints;
        struct addrinfo *results = 0;

        zero_bytes(&hints, sizeof(hints));
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;     
        hints.ai_protocol = IPPROTO_UDP;

        int gai_ok = getaddrinfo(host, service, &hints, &results);
        if (gai_ok != 0 || results == 0) return -1;

        
        struct addrinfo *p = results;
        while (p != 0) {
            int fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (fd >= 0) {
                (void)set_recv_timeout_20ms(fd); 

                
                rudp_packet_t syn_pkt;
                zero_bytes(&syn_pkt, sizeof(syn_pkt));
                syn_pkt.type = RUDP_SYN;

                
                int connected = 0;
                while (connected == 0) {
                    
                    (void)sendto(fd,
                                 &syn_pkt,
                                 sizeof(syn_pkt),
                                 0,
                                 p->ai_addr,
                                 (socklen_t)p->ai_addrlen);

                   
                    rudp_packet_t reply;
                    struct sockaddr_storage from;
                    socklen_t fromlen = sizeof(from);

                    ssize_t r = recvfrom(fd,
                                         &reply,
                                         sizeof(reply),
                                         0,
                                         (struct sockaddr*)&from,
                                         &fromlen);

                    if (r < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK ||
                            errno == ETIMEDOUT || errno == EINTR) {

                        } else {

                        }
                    } else {

                        int is_syn = (reply.type & RUDP_SYN) ? 1 : 0;
                        int is_ack = (reply.type & RUDP_ACK) ? 1 : 0;
                        if (is_syn == 1 && is_ack == 1) {

                            int saved = rudp_save_peer(fd, (struct sockaddr*)&from, fromlen);
                            if (saved == 0) {

                                rudp_packet_t ack_pkt;
                                zero_bytes(&ack_pkt, sizeof(ack_pkt));
                                ack_pkt.type = RUDP_ACK;

                                (void)sendto(fd,
                                             &ack_pkt,
                                             sizeof(ack_pkt),
                                             0,
                                             (struct sockaddr*)&from,
                                             fromlen);

                                final_fd = fd;
                                connected = 1;
                            } else {

                            }
                        } else {

                        }
                    }
                }

                if (final_fd >= 0) {
                    p = 0; 
                } else {
                    close(fd);
                    if (p != 0) p = p->ai_next;
                }
            } else {
                p = p->ai_next;
            }
        }

        freeaddrinfo(results);
        return final_fd;
    }

 
    return -1;
}

int sans_accept(const char *iface, int port, int protocol) {
    
    char service[12];
    if (port_to_str(service, port) != 0) return -1;

    
    if (protocol == IPPROTO_TCP) {
        struct addrinfo hints;
        struct addrinfo *results = 0;
        int listen_fd = -1;
        int client_fd = -1;

        zero_bytes(&hints, sizeof(hints));
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags    = AI_PASSIVE;

        int gai_ok = getaddrinfo(iface, service, &hints, &results);
        if (gai_ok != 0 || results == 0) return -1;

        struct addrinfo *p = results;
        while (p != 0) {
            int fd_try = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (fd_try >= 0) {
                int yes = 1;
                (void)setsockopt(fd_try, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

                int bound = bind(fd_try, p->ai_addr, p->ai_addrlen);
                if (bound == 0) {
                    int can_listen = listen(fd_try, 16);
                    if (can_listen == 0) {
                        listen_fd = fd_try;
                        break;
                    } else {
                        close(fd_try);
                    }
                } else {
                    close(fd_try);
                }
            }
            p = p->ai_next;
        }

        freeaddrinfo(results);
        if (listen_fd < 0) return -1;

        client_fd = accept(listen_fd, 0, 0);
        if (client_fd < 0) return -1;
        return client_fd;
    }

    
#ifdef IPPROTO_RUDP
    if (protocol == IPPROTO_RUDP)
#else
    if (protocol != IPPROTO_TCP)
#endif
    {
        struct addrinfo hints;
        struct addrinfo *results = 0;

        zero_bytes(&hints, sizeof(hints));
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;   
        hints.ai_protocol = IPPROTO_UDP;
        hints.ai_flags    = AI_PASSIVE;

        int gai_ok = getaddrinfo(iface, service, &hints, &results);
        if (gai_ok != 0 || results == 0) return -1;

        int fd = -1;
        struct addrinfo *p = results;
        while (p != 0) {
            int t = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (t >= 0) {
                int yes = 1;
                (void)setsockopt(t, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

                int bound = bind(t, p->ai_addr, p->ai_addrlen);
                if (bound == 0) {
                    fd = t;
                    break;
                } else {
                    close(t);
                }
            }
            p = p->ai_next;
        }

        freeaddrinfo(results);
        if (fd < 0) return -1;

        (void)set_recv_timeout_20ms(fd);

        
        struct sockaddr_storage from;
        socklen_t fromlen = sizeof(from);
        rudp_packet_t first;
        int got_syn = 0;

        while (got_syn == 0) {
            ssize_t r = recvfrom(fd,
                                 &first,
                                 sizeof(first),
                                 0,
                                 (struct sockaddr*)&from,
                                 &fromlen);

            if (r < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK ||
                    errno == ETIMEDOUT || errno == EINTR) {
                    
                } else {
                    
                }
            } else {
                int is_syn = (first.type & RUDP_SYN) ? 1 : 0;
                if (is_syn == 1) {
                    got_syn = 1;
                } else {
                    
                }
            }
        }

        
        if (rudp_save_peer(fd, (struct sockaddr*)&from, fromlen) != 0) return -1;

        
        rudp_packet_t synack;
        zero_bytes(&synack, sizeof(synack));
        synack.type = (RUDP_SYN | RUDP_ACK);

        int done = 0;
        while (done == 0) {
            (void)sendto(fd,
                         &synack,
                         sizeof(synack),
                         0,
                         (struct sockaddr*)&from,
                         fromlen);

            
            rudp_packet_t maybe;
            struct sockaddr_storage tmp;
            socklen_t tmplen = sizeof(tmp);

            ssize_t rr = recvfrom(fd,
                                  &maybe,
                                  sizeof(maybe),
                                  0,
                                  (struct sockaddr*)&tmp,
                                  &tmplen);

            if (rr < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK ||
                    errno == ETIMEDOUT || errno == EINTR) {
                    
                } else {
                    
                }
            } else {
                
                done = 1;
            }
        }

        return fd; 
    }

    return -1; 
}

int sans_disconnect(int fd) {
    if (fd < 0) return -1;
    return close(fd);
}
