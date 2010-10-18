#include <sys/syscalls.h>
#include <service/logging.h>
#include <service/string.h> //memcpy

extern "C" {
  #include "lwip/sys.h"
  #include "lwip/pbuf.h"
  #include "lwip/ip.h"
  #include "netif/etharp.h"
  #include "lwip/dhcp.h"
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
  netif->output  = etharp_output;
  netif->linkoutput = nul_lwip_netif_output;
  netif->flags   = netif->flags | NETIF_FLAG_ETHARP;
  netif->mtu     = 1500;
  netif->hwaddr_len = ETHARP_HWADDR_LEN;

  return ERR_OK;
}

extern "C"
void nul_lwip_init(void (*send_network)(char unsigned const * data, unsigned len), unsigned long long mac)
{
  __send_network = send_network;

  ip_addr_t _ipaddr, _netmask, _gw;
  IP4_ADDR(&_gw, 0,0,0,0);
  IP4_ADDR(&_ipaddr, 0,0,0,0);
  IP4_ADDR(&_netmask, 0,0,0,0);

  netif_add(&nul_netif, &_ipaddr, &_netmask, &_gw, NULL, nul_lwip_netif_init, ethernet_input);

  memcpy(nul_netif.hwaddr, &mac, ETHARP_HWADDR_LEN);
}

extern "C" void nul_lwip_dhcp_start(void) {
  dhcp_start(&nul_netif);
}

extern "C"
void nul_lwip_input(void * data, unsigned size) {
  struct pbuf *lwip_buf;
/*
  lwip_buf = pbuf_alloc(PBUF_RAW, size, PBUF_REF);
  if (!lwip_buf)
    Logging::panic("pbuf allocation failed size=%x data=%p\n", size, data);
*/
  lwip_buf = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
  if (!lwip_buf)
    Logging::panic("pbuf allocation failed size=%x data=%p\n", size, data);
  pbuf_take(lwip_buf, data, size);

  lwip_buf->payload = data;
  ethernet_input(lwip_buf, &nul_netif);

}

/*
void sys_sem_signal(sys_sem_t * sem) {

  if (!sem)
    lwip_print("error: sem up - pointer is zero\n", true);
 
  unsigned char res = nova_semup(*sem);
  if (res)
    lwip_print("error: sem up\n", true);
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout) {
  
  if (!sem)
    lwip_print("error: sem down - pointer is zero\n", true);
  if (timeout)
    lwip_print("error: sem down - timeouts are not supported\n", true);

  unsigned char res = nova_semdown(*sem);
  if (res)
    lwip_print("error: sem up\n", true);

  return timeout;
}

err_t sys_sem_new(sys_sem_t *sem, u8_t count) {
  static unsigned cap = 0x10000; //XXX alloc_cap();
  unsigned i;

  if (nova_create_sm(++cap))
    return ERR_VAL;

  for (i=0; i < count; i++)
    if (nova_semup(cap)) {
      if (nova_revoke(Crd(cap, 0, DESC_RIGHTS_ALL), true))
        lwip_print("error: release of semaphore cap\n", true);

      return ERR_VAL;
    }
  Logging::printf("count %u\n", count);

  *sem = cap;

  return ERR_OK;
}

*/
