#define DAT 0
#define SYN 1
#define ACK 2
#define FIN 4

typedef struct {
  char type;
  int seqnum;
  char payload[];
} rudp_packet_t;
