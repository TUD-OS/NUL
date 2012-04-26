/**
 * RawSocket Connector.
 * Bernhard Kauer <bk@vmmon.org>
 */
#include <poll.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <stdio.h>
#include <string.h>

const size_t buffer_size = 1<<22;
const size_t frame_size  = 1<<14;

#define CHECK(X) ({ int res; res = X; if (res < 0) { perror(#X); return -1; }; res; })

int main (int argv, char **args)
{
  int fd = CHECK(socket (PF_PACKET, SOCK_RAW, 0));

  struct tpacket_req r = { buffer_size, 1, frame_size, buffer_size/frame_size};
  CHECK(setsockopt(fd, SOL_PACKET, PACKET_RX_RING, &r, sizeof(r)));

  unsigned char *mem = static_cast<unsigned char *>(mmap(NULL, r.tp_block_size * r.tp_block_nr, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0));
  if (mem == MAP_FAILED) { perror("map"); return -1; }

  struct sockaddr_ll addr;
  memset(&addr, 0, sizeof(addr));
  addr.sll_family = AF_PACKET;
  addr.sll_protocol = htons(ETH_P_ALL);

  if (argv > 1) {
    struct ifreq i;
    memset (&i, 0, sizeof (i));
    snprintf (i.ifr_name, IFNAMSIZ, "%s", args[1]);
    CHECK (ioctl (fd, SIOCGIFINDEX, &i));
    addr.sll_ifindex=i.ifr_ifindex;
  }
  CHECK(bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)));

  for (unsigned char *ptr = mem;;) {
    while (*reinterpret_cast<volatile unsigned long *>(ptr)) {
      struct tpacket_hdr *h = reinterpret_cast<tpacket_hdr *>(ptr);
      struct sockaddr_ll *sll= reinterpret_cast<struct sockaddr_ll *>(reinterpret_cast<char *>(h) + TPACKET_ALIGN(sizeof(struct tpacket_hdr)));
      unsigned char *packet= reinterpret_cast<unsigned char *>(h) + h->tp_mac;

      printf("%u.%.6u: if%u %d %d %d %u bytes %02x:%02x:%02x:%02x:%02x:%02x\n",
	     h->tp_sec, h->tp_usec,
	     sll->sll_ifindex,
	     sll->sll_pkttype,
	     h->tp_mac, h->tp_net,
	     h->tp_len, packet[0], packet[1], packet[2], packet[3], packet[4], packet[5]);
      asm volatile ("" : : : "memory");
      h->tp_status = 0;

      ptr += frame_size;
      if (ptr > mem + buffer_size) ptr = mem;
    }

    // wait for new packets to arrive
    struct pollfd p = {fd, POLLIN, 0 };
    CHECK(poll(&p, 1, 10));
    if (p.revents & POLLERR) {
      printf("error on poll\n");
      return 1;
    }
  }
  return 0;
}
