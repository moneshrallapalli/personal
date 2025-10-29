#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include "include/rudp.h"

#ifndef RUDP_ADDRBOOK_CAP
#define RUDP_ADDRBOOK_CAP 128
#endif

typedef struct {
    int sock;
    struct sockaddr_storage addr;
    socklen_t addrlen;
    int in_use;
} addr_entry_t;

static addr_entry_t g_addrbook[RUDP_ADDRBOOK_CAP];


static void copy_bytes(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dst;
    const unsigned char *s = (const unsigned char*)src;
    size_t i = 0;
    while (i < n) { d[i] = s[i]; i = i + 1; }
}

static int addrbook_find_existing(int sock) {
    int i = 0;
    while (i < RUDP_ADDRBOOK_CAP) {
        if (g_addrbook[i].in_use == 1) {
            if (g_addrbook[i].sock == sock) {
                return i;
            }
        }
        i = i + 1;
    }
    return -1;
}

static int addrbook_find_free(void) {
    int i = 0;
    while (i < RUDP_ADDRBOOK_CAP) {
        if (g_addrbook[i].in_use == 0) {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

static int addrbook_set(int sock, const struct sockaddr *sa, socklen_t slen) {
    if (sa == 0) { errno = EINVAL; return -1; }
    if ((size_t)slen > (size_t)sizeof(struct sockaddr_storage)) {
        errno = EINVAL; return -1;
    }

    
    int idx = addrbook_find_existing(sock);
    if (idx >= 0) {
        copy_bytes(&g_addrbook[idx].addr, sa, (size_t)slen);
        g_addrbook[idx].addrlen = slen;
        return 0;
    }

    
    int free_idx = addrbook_find_free();
    if (free_idx >= 0) {
        g_addrbook[free_idx].in_use = 1;
        g_addrbook[free_idx].sock   = sock;
        copy_bytes(&g_addrbook[free_idx].addr, sa, (size_t)slen);
        g_addrbook[free_idx].addrlen = slen;
        return 0;
    }

    errno = ENOSPC;
    return -1;
}

static int addrbook_get(int sock, struct sockaddr *sa, socklen_t *salen) {
    if (sa == 0 || salen == 0) { errno = EINVAL; return -1; }

    int idx = addrbook_find_existing(sock);
    if (idx < 0) { errno = ENOENT; return -1; }

    if (*salen < g_addrbook[idx].addrlen) { errno = EINVAL; return -1; }

    copy_bytes(sa, &g_addrbook[idx].addr, (size_t)g_addrbook[idx].addrlen);
    *salen = g_addrbook[idx].addrlen;
    return 0;
}



extern void enqueue_packet(int sock, const char* buf, int len);
extern int send_seqnum;
extern int recv_seqnum;


int rudp_save_peer(int sock, const struct sockaddr *sa, socklen_t slen) {
    return addrbook_set(sock, sa, slen);
}


int sans_send_pkt(int socket, const char* buf, int len) {
    struct sockaddr_storage peer_addr;
    socklen_t peer_len = (socklen_t)sizeof(peer_addr);

    int ok = addrbook_get(socket, (struct sockaddr*)&peer_addr, &peer_len);
    if (ok != 0) {
        if (errno == ENOENT) { errno = EDESTADDRREQ; }
        return -1;
    }

    
    enqueue_packet(socket, buf, len);

    return len;
}


int sans_recv_pkt(int socket, char* buf, int len) {
    char pkt_buf[1024];
    struct sockaddr_storage src_addr;
    socklen_t src_len = sizeof(src_addr);

    
    while (1) {
        
        ssize_t recv_bytes = recvfrom(socket, pkt_buf, sizeof(pkt_buf), 0,
                                       (struct sockaddr*)&src_addr, &src_len);

        if (recv_bytes <= 0) {
            return -1;
        }

        
        rudp_packet_t* pkt = (rudp_packet_t*)pkt_buf;

        
        if (pkt->seqnum != recv_seqnum) {
            
            rudp_packet_t ack_pkt = {0};
            ack_pkt.type = ACK;
            
            ack_pkt.seqnum = recv_seqnum - 1;
            sendto(socket, (void*)&ack_pkt, sizeof(ack_pkt), 0,
                   (struct sockaddr*)&src_addr, src_len);
            
            continue;
        }

        
        int payload_len = recv_bytes - sizeof(rudp_packet_t);
        if (payload_len > len) {
            payload_len = len;
        }
        if (payload_len > 0) {
            memcpy(buf, pkt->payload, payload_len);
        }

        
        rudp_packet_t ack_pkt = {0};
        ack_pkt.type = ACK;
        ack_pkt.seqnum = recv_seqnum;
        sendto(socket, (void*)&ack_pkt, sizeof(ack_pkt), 0,
               (struct sockaddr*)&src_addr, src_len);

        
        recv_seqnum++;

        return payload_len;
    }
}
