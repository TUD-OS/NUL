/*
 * (C) 2011-2012 Alexander Boettcher
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL (NOVA user land).
 *
 * NUL is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <service/logging.h>
#include <service/string.h> //memcpy
#include <service/helper.h> //assert

BEGIN_EXTERN_C
  #include "lwip/sys.h"
  #include "lwip/pbuf.h"
  #include "lwip/ip.h"
  #include "netif/etharp.h"
  #include "lwip/dhcp.h"
  #include "lwip/tcp.h"
  #include "lwip/tcp_impl.h"

 void lwip_init(void);

void lwip_print(const char *format, ...) {

  va_list ap;
  va_start(ap, format);
  Logging::vprintf(format, ap);
  va_end(ap);

}

__attribute__((noreturn))
void lwip_assert(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  Logging::vprintf(format, ap);
  va_end(ap);

  Logging::panic("panic\n");
}

END_EXTERN_C

void sys_init(void) { }

typedef void (*fn_recv_call_t)(uint32 remoteip, uint16 remoteport, uint16 localport, void * in_data, size_t in_len);
typedef void (*fn_connected_call_t)(bool);
struct nul_tcp_struct {
  bool outgoing;
  u16_t port;
  struct tcp_pcb * listening_pcb;
  struct tcp_pcb * openconn_pcb;
  fn_recv_call_t fn_recv_call;
  fn_connected_call_t fn_connected;
} nul_tcps[4];

/*
 * time
 */
u32_t sys_now(void) {
  Logging::printf("        - sys_now unimpl.\n"); return 0;
}

/*
 * netif 
 */
static struct netif nul_netif;
static void (*__send_network)(char unsigned const * data, unsigned len);

static char * snd_buf;
static unsigned long snd_buf_size = 1; //in pages

static err_t
nul_lwip_netif_output(struct netif *netif, struct pbuf *p)
{
  if (!p->next) {
    __send_network(reinterpret_cast<unsigned char const *>(p->payload), p->len);
    return ERR_OK;
  }

  if (snd_buf && (p->tot_len / 4096) > snd_buf_size) {
    delete [] snd_buf;
    snd_buf = 0;
    snd_buf_size = p->tot_len / 4096;
  }
  if (!snd_buf) snd_buf = new (4096) char[snd_buf_size * 4096];
  if (!snd_buf) return ERR_MEM;
  char * pos = snd_buf;

  while (p) {
    memcpy(pos, p->payload, p->len);
    pos += p->len;
    p = p->next;
  }

  __send_network(reinterpret_cast<unsigned char const *>(snd_buf), pos - snd_buf);
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

BEGIN_EXTERN_C

bool nul_ip_init(void (*send_network)(char unsigned const * data, unsigned len), unsigned long long mac)
{
  lwip_init();

  __send_network = send_network;

  ip_addr_t _ipaddr, _netmask, _gw;
  memset(&_gw, 0, sizeof(_gw));
  memset(&_ipaddr, 0, sizeof(_ipaddr));
  memset(&_netmask, 0, sizeof(_netmask));

  if (&nul_netif != netif_add(&nul_netif, &_ipaddr, &_netmask, &_gw, NULL, nul_lwip_netif_init, ethernet_input))
    return false;

  netif_set_default(&nul_netif);

  memcpy(nul_netif.hwaddr, &mac, ETHARP_HWADDR_LEN);

  return true;
}

void nul_ip_input(void * data, unsigned size) {
  struct pbuf *lwip_buf;

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
END_EXTERN_C

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

static
bool nul_ip_udp(unsigned _port) {
  struct udp_pcb * udp_pcb = udp_new();
  if (!udp_pcb) Logging::panic("udp new failed\n");

  ip_addr _ipaddr;
  memset(&_ipaddr, 0, sizeof(_ipaddr));
  u16_t port = _port;

  err_t err = udp_bind(udp_pcb, &_ipaddr, port);
  if (err != ERR_OK) Logging::panic("udp bind failed\n");

  void * recv_arg = 0;
  udp_recv(udp_pcb, nul_udp_recv, recv_arg); //set callback

  return true;
}

/*
 * TCP stuff
 */
#define NUL_TCP_EOF (~0u)

BEGIN_EXTERN_C
static void nul_tcp_close(struct nul_tcp_struct * tcp_struct) {
  Logging::printf("[tcp]   - %s connection closed - port %u\n", tcp_struct->outgoing ? "outgoing" : "incoming", tcp_struct->port);
  if (tcp_struct->fn_recv_call && tcp_struct->openconn_pcb) {
    tcp_struct->fn_recv_call(tcp_struct->openconn_pcb->remote_ip.addr,
                             tcp_struct->openconn_pcb->remote_port, tcp_struct->openconn_pcb->local_port,
                             NULL, NUL_TCP_EOF);
  }
  if (tcp_struct->outgoing) //outgoing call, either connected or unconnected
    tcp_struct->listening_pcb = 0; //free old port - it was a outgoing connection
  else
    if (tcp_struct->listening_pcb)
      tcp_accepted(tcp_struct->listening_pcb); //acknowledge now the old listen pcb to be able to get new connections of same port XXX

  tcp_struct->openconn_pcb = 0;
}

static err_t nul_tcp_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  static unsigned long total = 0;
  struct nul_tcp_struct * tcp_struct = reinterpret_cast<struct nul_tcp_struct *>(arg);

  if (p) {

    if (!tcp_struct || !tcp_struct->fn_recv_call) {
      total += p->tot_len;
      Logging::printf("[tcp]   - %u.%u.%u.%u:%u -> %u - len %u, first part %u - total %lu\n",
                      (tpcb->remote_ip.addr) & 0xff, (tpcb->remote_ip.addr >> 8) & 0xff,
                      (tpcb->remote_ip.addr >> 16) & 0xff, (tpcb->remote_ip.addr >> 24) & 0xff,
                      tpcb->remote_port, tpcb->local_port, p->tot_len, p->len, total);
    }

    if (tcp_struct && tcp_struct->fn_recv_call)
      tcp_struct->fn_recv_call(tpcb->remote_ip.addr, tpcb->remote_port, tpcb->local_port, p->payload, p->len);

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
  } else {
    total = 0;
    if (tcp_struct) nul_tcp_close(tcp_struct);
    else Logging::printf("[tcp]   - warning - some struct is NULL\n");
  }

  return ERR_OK;
}

static void nul_tcp_error(void *arg, err_t err) {
  struct nul_tcp_struct * tcp_struct = reinterpret_cast<struct nul_tcp_struct *>(arg);
  if (tcp_struct) {
    if ((ERR_RST == err))
      nul_tcp_close(tcp_struct);
    else
      Logging::printf("[tcp]   - warning - err %d, port %u, pcb_open=%p, pcb_listing=%p\n", err, tcp_struct->port, tcp_struct->openconn_pcb, tcp_struct->listening_pcb);
    if (tcp_struct->fn_connected) tcp_struct->fn_connected(false); //signal that attempt failed

  } else
    Logging::printf("[tcp]   - warning - err arg=%p err=%d\n", arg, err);
}
END_EXTERN_C

static err_t nul_tcp_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
  struct nul_tcp_struct * tcp_struct = reinterpret_cast<struct nul_tcp_struct *>(arg);

  if (tcp_struct->openconn_pcb > 0) {
    Logging::printf("error   - we accept only one connection (%u.%u.%u.%u:%u -> %u)\n",
                    (newpcb->remote_ip.addr) & 0xff, (newpcb->remote_ip.addr >> 8) & 0xff,
                    (newpcb->remote_ip.addr >> 16) & 0xff, (newpcb->remote_ip.addr >> 24) & 0xff,
                    newpcb->remote_port, newpcb->local_port);
    return ERR_ISCONN;          // We allow only one connection
  }

  tcp_recv(newpcb, nul_tcp_recv); 
  tcp_arg(newpcb, tcp_struct);
  tcp_err(newpcb, nul_tcp_error);

  tcp_struct->openconn_pcb = newpcb; //XXX

  Logging::printf("[tcp]   - connection from %u.%u.%u.%u:%u -> %u\n",
                   (newpcb->remote_ip.addr) & 0xff, (newpcb->remote_ip.addr >> 8) & 0xff,
                   (newpcb->remote_ip.addr >> 16) & 0xff, (newpcb->remote_ip.addr >> 24) & 0xff,
                    newpcb->remote_port, newpcb->local_port);

  //if (tcp_struct->listening_pcb != tcp_struct->openconn_pcb)
  //  tcp_accepted(tcp_struct->listening_pcb); //we support for the moment only one incoming connection of the same port XXX

  return ERR_OK;
}

static err_t callback_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
  struct nul_tcp_struct * tcp_struct = reinterpret_cast<struct nul_tcp_struct *>(arg);

  if (err == ERR_OK) {
    assert(tpcb);
    assert(arg);
    assert(tcp_struct->listening_pcb == tpcb);

    tcp_struct->openconn_pcb = tpcb;
/*
    Logging::printf("[tcp]   - outgoing connection from %u.%u.%u.%u:%u - established\n",
                     (tpcb->local_ip.addr) & 0xff, (tpcb->local_ip.addr >> 8) & 0xff,
                     (tpcb->local_ip.addr >> 16) & 0xff, (tpcb->local_ip.addr >> 24) & 0xff,
                      tpcb->local_port);
*/
  } else
    Logging::printf("[tcp]   - error for outgoing connection arg=%p pcb=%p err=%d\n", arg, tpcb, err);

  if (tcp_struct->fn_connected)
    tcp_struct->fn_connected(err == ERR_OK);

  return ERR_OK;
}

static
bool nul_ip_tcp(unsigned long * _port, fn_recv_call_t fn_call_me, ip_addr * addr, fn_connected_call_t fn_conn_me) {
  struct tcp_pcb * tmp_pcb = tcp_new();
  if (!tmp_pcb) { Logging::printf("[tcp]   - new failed\n"); return false; }
  u16_t port = *_port;

  if (addr && addr->addr != 0) {
    //set callbacks
    tcp_err(tmp_pcb, nul_tcp_error);

    err_t err = tcp_connect(tmp_pcb, addr, port, callback_connected);
    Logging::printf("[tcp]   - trying to connect %u.%u.%u.%u:%u -> %u.%u.%u.%u:%u - err=%d\n",
                     (tmp_pcb->local_ip.addr) & 0xff, (tmp_pcb->local_ip.addr >> 8) & 0xff,
                     (tmp_pcb->local_ip.addr >> 16) & 0xff, (tmp_pcb->local_ip.addr >> 24) & 0xff,
                     tmp_pcb->local_port,
                     (tmp_pcb->remote_ip.addr) & 0xff, (tmp_pcb->remote_ip.addr >> 8) & 0xff,
                     (tmp_pcb->remote_ip.addr >> 16) & 0xff, (tmp_pcb->remote_ip.addr >> 24) & 0xff,
                      tmp_pcb->remote_port, err);
    if (err != ERR_OK) return false;

    for (unsigned i=0; i < sizeof(nul_tcps) / sizeof(nul_tcps[0]); i++)
      if (nul_tcps[i].listening_pcb == 0) {
        nul_tcps[i].outgoing      = true;
        nul_tcps[i].port          = tmp_pcb->local_port;
        nul_tcps[i].listening_pcb = tmp_pcb;
        nul_tcps[i].openconn_pcb  = 0;
        nul_tcps[i].fn_recv_call  = fn_call_me;
        nul_tcps[i].fn_connected  = fn_conn_me;

        //set callback
        tcp_arg(tmp_pcb, &nul_tcps[i]);
        tcp_recv(tmp_pcb, nul_tcp_recv);

        *_port = tmp_pcb->local_port;
        return true;
      }

    return false;
  } else {
    ip_addr _ipaddr;
    memset(&_ipaddr, 0, sizeof(_ipaddr));

    err_t err = tcp_bind(tmp_pcb, &_ipaddr, port);
    if (err != ERR_OK) { Logging::printf("failure - tcp bind\n"); return false; }

    struct tcp_pcb * listening_pcb = tcp_listen(tmp_pcb);
    if (!listening_pcb) { Logging::printf("failure - tcp listen\n"); return false; }

    //set callbacks
    for (unsigned i=0; i < sizeof(nul_tcps) / sizeof(nul_tcps[0]); i++)
      if (nul_tcps[i].listening_pcb == 0) {
        nul_tcps[i].outgoing      = false;
        nul_tcps[i].port          = port;
        nul_tcps[i].listening_pcb = listening_pcb;
        nul_tcps[i].openconn_pcb  = 0;
        nul_tcps[i].fn_recv_call  = fn_call_me;
        nul_tcps[i].fn_connected  = 0;

        tcp_arg(listening_pcb, &nul_tcps[i]);
        tcp_accept(listening_pcb, nul_tcp_accept);

        return true;
      }
  }

  return false;
}

static struct nul_tcp_struct * lookup_internal(u16_t port) {
  for (unsigned i=0; i < sizeof(nul_tcps) / sizeof(nul_tcps[0]); i++) {
    if (nul_tcps[i].listening_pcb != 0 && nul_tcps[i].port == port)
      return &nul_tcps[i];
  }
  return 0;
}

static struct tcp_pcb * lookup(u16_t port) {
  for (unsigned i=0; i < sizeof(nul_tcps) / sizeof(nul_tcps[0]); i++) {
    if (nul_tcps[i].listening_pcb != 0 && nul_tcps[i].port == port)
      return nul_tcps[i].openconn_pcb;
  }
  return 0;
}

EXTERN_C
bool nul_ip_config(unsigned para, void * arg) {
  static unsigned long long etharp_timer = ETHARP_TMR_INTERVAL;
  static unsigned long long dhcp_fine_timer = DHCP_FINE_TIMER_MSECS;
  static unsigned long long dhcp_coarse_timer = DHCP_COARSE_TIMER_MSECS;
  static unsigned long long tcp_fast_interval = TCP_FAST_INTERVAL;
  static unsigned long long tcp_slow_interval = TCP_SLOW_INTERVAL;

  static ip_addr last_ip, last_mask, last_gw;

  switch (para) {
    case 0: /* ask for version of this implementation */
      if (!arg) return false;
      *reinterpret_cast<unsigned long long *>(arg) = 0x5;
      return true;
    case 1: /* enable dhcp to get an ip address */
      dhcp_start(&nul_netif);
      return true;
    case 2: /* dump ip addr to screen */
      if (last_ip.addr   != nul_netif.ip_addr.addr ||
          last_mask.addr != nul_netif.netmask.addr ||
          last_gw.addr   != nul_netif.gw.addr)
      {
        last_ip.addr   = nul_netif.ip_addr.addr;
        last_mask.addr = nul_netif.netmask.addr;
        last_gw.addr   = nul_netif.gw.addr;
        Logging::printf("update  - got ip=%u.%u.%u.%u mask=%u.%u.%u.%u gw=%u.%u.%u.%u\n",
                        last_ip.addr & 0xff,
                        (last_ip.addr >> 8) & 0xff,
                        (last_ip.addr >> 16) & 0xff,
                        (last_ip.addr >> 24) & 0xff,
                        last_mask.addr & 0xff,
                        (last_mask.addr >> 8) & 0xff,
                        (last_mask.addr >> 16) & 0xff,
                        (last_mask.addr >> 24) & 0xff,
                        last_gw.addr & 0xff,
                        (last_gw.addr >> 8) & 0xff,
                        (last_gw.addr >> 16) & 0xff,
                        (last_gw.addr >> 24) & 0xff);
        return true;
      }
      return false;
    case 3: /* return next timeout to be fired */
    {
      if (!arg) return false;

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
      if (!tcp_fast_interval) {
        tcp_fast_interval = TCP_FAST_INTERVAL;
        tcp_fasttmr();
      }
      if (!tcp_slow_interval) {
        tcp_slow_interval = TCP_SLOW_INTERVAL;
        tcp_slowtmr();
      }

//      unsigned long long next = etharp_timer < dhcp_fine_timer ? etharp_timer : dhcp_fine_timer;
//      next = next < dhcp_coarse_timer ? next : dhcp_coarse_timer;
      unsigned long long next = MIN(etharp_timer, dhcp_fine_timer);
      next = MIN(next, dhcp_coarse_timer);
      next = MIN(next, tcp_fast_interval);
      next = MIN(next, tcp_slow_interval);

      etharp_timer      -= next;
      dhcp_fine_timer   -= next;
      dhcp_coarse_timer -= next;
      tcp_fast_interval -= next;
      tcp_slow_interval -= next;

      *reinterpret_cast<unsigned long long *>(arg) = next;
      return true;
    }
    case 4: /* open udp connection */
    {
      if (!arg) return false;

      unsigned port = *reinterpret_cast<unsigned *>(arg);
      return nul_ip_udp(port);
      break;
    }
    case 5: /* open tcp connection */
    {
      if (!arg) return false;

      assert(sizeof(unsigned long) == sizeof(void *));
      unsigned long       * port      = reinterpret_cast<unsigned long *>(arg);
      fn_recv_call_t      * call_recv = reinterpret_cast<fn_recv_call_t *>(port + 1);
      ip_addr_t           * addr      = reinterpret_cast<ip_addr_t *>(call_recv + 1);
      fn_connected_call_t * call_conn = reinterpret_cast<fn_connected_call_t *>(addr + 1);
      return nul_ip_tcp(port, *call_recv, addr, *call_conn);
      break;
    }
    case 6: /* set ip addr */
    {
      if (!arg) return false;

      ip_addr_t * value = reinterpret_cast<ip_addr_t *>(arg);
      //netif_set_addr(nul_netif, ip_addr_t *ipaddr, ip_addr_t *netmask, ip_addr_t *gw);
      netif_set_down(&nul_netif);
      netif_set_addr(&nul_netif, value, value + 1, value + 2);
      netif_set_up(&nul_netif);
      return true;
    }
    case 7: /* send tcp packet */
    {
      if (!arg) return false;
      assert(sizeof(unsigned long) == sizeof(void *));

      struct arge {
        unsigned long port;
        size_t count;
        void * data;
      } * arge;
      arge = reinterpret_cast<struct arge *>(arg);

      struct tcp_pcb * tpcb = lookup(arge->port);
      if (!tpcb) { Logging::printf("[tcp]   - no connection via port %lu, sending packet failed\n", arge->port); return false; }
      err_t err = tcp_write(tpcb, arge->data, arge->count, 0); //may be not send immediately
      if (err != ERR_OK) Logging::printf("[tcp]   - connection available, sending packet failed\n");
      tcp_output(tpcb); //force to send immediately
      return (err == ERR_OK);
      break;
    }
    case 8: /* close */
    {
      if (!arg) return false;
      uint16 port = *reinterpret_cast<uint16 *>(arg);
      struct nul_tcp_struct * tcp_struct = lookup_internal(port);
      //Logging::printf("close tpcb %p\n", tcp_struct);
      if (tcp_struct) {
        if (tcp_struct->openconn_pcb) tcp_close(tcp_struct->openconn_pcb);
        nul_tcp_close(tcp_struct);
      }
      break;
    }
    default:
      Logging::panic("unknown parameter %u\n", para);
  }
  return false;
}
