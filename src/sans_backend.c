#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "include/rudp.h"

const unsigned int swnd_size = 1;

typedef struct {
  int socket;
  int packetlen;
  rudp_packet_t* packet;
} swnd_entry_t;

swnd_entry_t* send_window;

static pthread_mutex_t send_window_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t send_window_cond = PTHREAD_COND_INITIALIZER;

int send_seqnum = 0;

int recv_seqnum = 0;

static int count = 0;
static int head = 0;

void enqueue_packet(int sock, const char* buf, int len) {
  pthread_mutex_lock(&send_window_lock);

  while (count >= (int)swnd_size) {
    pthread_cond_wait(&send_window_cond, &send_window_lock);
  }

  int idx = head;

  int total_size = sizeof(rudp_packet_t) + len;
  send_window[idx].packet = (rudp_packet_t*)malloc(total_size);

  if (send_window[idx].packet == NULL) {
    pthread_mutex_unlock(&send_window_lock);
    return;
  }

  send_window[idx].packet->type = DAT;

  memcpy(send_window[idx].packet->payload, buf, len);
  send_window[idx].socket = sock;
  send_window[idx].packetlen = total_size;

  count += 1;

  pthread_mutex_unlock(&send_window_lock);
}

static void dequeue_packet(void) {
  pthread_mutex_lock(&send_window_lock);

  if (send_window[head].packet != NULL) {
    free(send_window[head].packet);
    send_window[head].packet = NULL;
  }

  count -= 1;

  pthread_cond_signal(&send_window_cond);

  pthread_mutex_unlock(&send_window_lock);
}

void* rudp_backend(void* unused) {
  send_window = malloc(sizeof(swnd_entry_t) * swnd_size);

  while (1) {
    pthread_mutex_lock(&send_window_lock);

    if (count > 0 && send_window[head].packet != NULL) {
      int sock = send_window[head].socket;
      rudp_packet_t* pkt = send_window[head].packet;
      int len = send_window[head].packetlen;

      pthread_mutex_unlock(&send_window_lock);

      pkt->seqnum = send_seqnum;

      ssize_t sent_bytes = sendto(sock, (void*)pkt, len, 0, NULL, 0);

      if (sent_bytes < 0) {
        usleep(10000);
        continue;
      }

      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = 100000;

      if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        usleep(10000);
        continue;
      }

      while (1) {
        char ack_buf[256];
        struct sockaddr_storage src_addr;
        socklen_t src_len = sizeof(src_addr);

        ssize_t recv_bytes = recvfrom(sock, ack_buf, sizeof(ack_buf), 0,
                                       (struct sockaddr*)&src_addr, &src_len);

        if (recv_bytes > 0) {
          rudp_packet_t* ack_pkt = (rudp_packet_t*)ack_buf;

          if (ack_pkt->type == ACK && ack_pkt->seqnum == send_seqnum) {
            send_seqnum++;
            dequeue_packet();
            break;
          }
        } else {
          break;
        }
      }
    } else {
      pthread_mutex_unlock(&send_window_lock);
      usleep(1000);
    }
  }

  return NULL;
}

int init_rudp_backend(void) {
  return 0;
}
