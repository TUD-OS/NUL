#include <sys/syscalls.h>
#include <service/logging.h>
#include <service/string.h> //memcpy

extern "C" {
  #include "lwip/sys.h"
  #include "lwip/pbuf.h"
  #include "lwip/ip.h"
  #include "netif/etharp.h"
  #include "lwip/dhcp.h"
  #include "lwip/tcp.h"
}

void sys_init(void) {
}

extern "C"
void lwip_print(const char *format, ...) {

  va_list ap;
  va_start(ap, format);
  Logging::vprintf(format, ap);
  va_end(ap);

}

extern "C"
__attribute__((noreturn))
void lwip_assert(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  Logging::vprintf(format, ap);
  va_end(ap);

  Logging::panic("panic\n");
}

/*
 * time
 */
u32_t sys_now(void) {
  Logging::printf("time\n"); return 0;
}

extern "C"
unsigned long long nul_lwip_netif_next_timer(void) {
  static unsigned long long etharp_timer = ETHARP_TMR_INTERVAL;
  static unsigned long long dhcp_fine_timer = DHCP_FINE_TIMER_MSECS;
  static unsigned long long dhcp_coarse_timer = DHCP_COARSE_TIMER_MSECS;

  if (!etharp_timer) {
    etharp_timer = ETHARP_TMR_INTERVAL;
    etharp_tmr();
  }
  if (!dhcp_fine_timer) {
    dhcp_fine_timer = DHCP_FINE_TIMER_MSECS;
    dhcp_fine_tmr();
  }
  if (!dhcp_coarse_timer) {
    dhcp_coarse_timer = DHCP_COARSE_TIMER_MSECS;
    dhcp_coarse_tmr();
  }
  unsigned long long next = etharp_timer < dhcp_fine_timer ? etharp_timer : dhcp_fine_timer;
  next = next < dhcp_coarse_timer ? next : dhcp_coarse_timer;

  etharp_timer -= next;
  dhcp_fine_timer -= next;
  dhcp_coarse_timer -= next;

  return next;
}

/*
 * netif 
 */
static struct netif nul_netif;
static void (*__send_network)(char unsigned const * data, unsigned len);

static err_t
nul_lwip_netif_output(struct netif *netif, struct pbuf *p)
{
  __send_network(reinterpret_cast<unsigned char const *>(p->payload), p->len);
  if (p->next)
    Logging::panic("unimpl. next\n");
  return ERR_OK;
}

static err_t
nul_lwip_netif_init(struct netif *netif)
{
  netif->name[0] = 'n';
  netif->name[1] = 'u';
  netif->num = 0;
  netif->output     = etharp_output;
  netif->linkoutput = nul_lwip_netif_output;
  netif->flags      = netif->flags | NETIF_FLAG_ETHARP;
  netif->mtu        = 1500;
  netif->hwaddr_len = ETHARP_HWADDR_LEN;

  return ERR_OK;
}

extern "C"
bool nul_lwip_init(void (*send_network)(char unsigned const * data, unsigned len), unsigned long long mac)
{
  __send_network = send_network;

  ip_addr_t _ipaddr, _netmask, _gw;
  IP4_ADDR(&_gw, 0,0,0,0);
  IP4_ADDR(&_ipaddr, 0,0,0,0);
  IP4_ADDR(&_netmask, 0,0,0,0);

  if (&nul_netif != netif_add(&nul_netif, &_ipaddr, &_netmask, &_gw, NULL, nul_lwip_netif_init, ethernet_input))
    return false;

  netif_set_default(&nul_netif);

  memcpy(nul_netif.hwaddr, &mac, ETHARP_HWADDR_LEN);

  return true;
}

extern "C" void nul_lwip_dhcp_start(void) {
  dhcp_start(&nul_netif);
}

extern "C" bool get_ip_addr(void) {
  static ip_addr last_ip_addr;
  
  if (last_ip_addr.addr != nul_netif.ip_addr.addr) {
    Logging::printf("ip addr changed = %u.%u.%u.%u\n",
      nul_netif.ip_addr.addr & 0xff,
      (nul_netif.ip_addr.addr >> 8) & 0xff,
      (nul_netif.ip_addr.addr >> 16) & 0xff,
      (nul_netif.ip_addr.addr >> 24) & 0xff);
    last_ip_addr.addr = nul_netif.ip_addr.addr;
    return true;
  }
  return false;
}

void debug_dump(char * test, unsigned len) {
    unsigned i = 0;
    while ( i + 8 <= len && i < 64) {
      Logging::printf("%02x %02x|%02x|%02x|%02x|%02x|%02x|%02x|%02x\n", i, test[i]&0xff, test[i+1]&0xff, test[i+2]&0xff, test[i+3]&0xff,
        test[i+4]&0xff, test[i+5]&0xff, test[i+6]&0xff, test[i+7]&0xff);
      i += 8;
    }
}
extern "C"
void nul_lwip_input(void * data, unsigned size) {
  struct pbuf *lwip_buf;

/*  if (size > 25)
    Logging::printf("paket %02x%02x %02x\n",
      *((char *)data + 12), *((char *)data + 13),
      *((char *)data + 14 + 9));
*/
  if (size > 25 && (*((char *)data + 12) == 0x8) && (*((char *)data + 13) == 0) && (*((char *)data + 14 + 9)) == 0x1) { //ICMP
    lwip_buf = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
    if (!lwip_buf) Logging::panic("pbuf allocation failed size=%x data=%p\n", size, data);
    pbuf_take(lwip_buf, data, size);
  } else {
    lwip_buf = pbuf_alloc(PBUF_RAW, size, PBUF_REF);
    if (!lwip_buf) Logging::panic("pbuf allocation failed size=%x data=%p\n", size, data);
    lwip_buf->payload = data;
  }

  ethernet_input(lwip_buf, &nul_netif);

  // don't free - fragmented packets stay for a while until all is received
  //  if (lwip_buf->ref) 
  //    pbuf_free(lwip_buf);
}

static void nul_udp_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p, struct ip_addr *remoteaddr, u16_t remoteport) {
  static unsigned long total = 0;

  if (!p) { Logging::printf("[udp] closing event ?\n"); total = 0; return; }

  total += p->tot_len;

  Logging::printf("[udp] %u.%u.%u.%u:%u - len %u, first part %u - total %lu\n",
                  (remoteaddr->addr) & 0xff, (remoteaddr->addr >> 8) & 0xff,
                  (remoteaddr->addr >> 16) & 0xff, (remoteaddr->addr >> 24) & 0xff,
                  remoteport, p->tot_len, p->len, total);

  pbuf_free(p);
}

extern "C" void nul_lwip_udp_test(void) {
  struct udp_pcb * udp_pcb = udp_new();
  if (!udp_pcb) Logging::panic("udp new failed\n");
// void udp_remove(struct udp_pcb *pcb)

  ip_addr _ipaddr;
  IP4_ADDR(&_ipaddr, 0,0,0,0);
  u16_t port = 5555;

  err_t err = udp_bind(udp_pcb, &_ipaddr, port);
  if (err != ERR_OK) Logging::panic("udp bind failed\n");

  void * recv_arg = 0;
  udp_recv(udp_pcb, nul_udp_recv, recv_arg); //set callback

}

/*
 * TCP stuff
 */
static err_t nul_tcp_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  static unsigned long total = 0;

  if (p) {
    total += p->tot_len;

    Logging::printf("[tcp] %u.%u.%u.%u:%u -> %u - len %u, first part %u - total %lu\n",
                    (tpcb->remote_ip.addr) & 0xff, (tpcb->remote_ip.addr >> 8) & 0xff,
                    (tpcb->remote_ip.addr >> 16) & 0xff, (tpcb->remote_ip.addr >> 24) & 0xff,
                    tpcb->remote_port, tpcb->local_port, p->tot_len, p->len, total);

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
  } else {
    total = 0;
    Logging::printf("[tcp] connection closed ? %p err %u\n", p, err);
  }

  return ERR_OK;
}

static err_t nul_tcp_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
  struct tcp_pcb * listening_pcb = reinterpret_cast<struct tcp_pcb *>(arg);

  tcp_recv(newpcb, nul_tcp_recv); 

  tcp_accepted(listening_pcb);

  return ERR_OK;
}

extern "C" void nul_lwip_tcp_test(void) {
  struct tcp_pcb * tmp_pcb = tcp_new();
  if (!tmp_pcb) Logging::panic("tcp new failed\n");

  ip_addr _ipaddr;
  IP4_ADDR(&_ipaddr, 0,0,0,0);
  u16_t port = 7777;

  err_t err = tcp_bind(tmp_pcb, &_ipaddr, port);
  if (err != ERR_OK) Logging::panic("tcp bind failed\n");

  struct tcp_pcb * listening_pcb = tcp_listen(tmp_pcb);
  if (!listening_pcb) Logging::panic("tcp listen failed\n");

  //set callbacks
  tcp_arg(listening_pcb, listening_pcb);
  tcp_accept(listening_pcb, nul_tcp_accept);

}
