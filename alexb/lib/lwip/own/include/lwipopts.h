#include <stddef.h> //size_t include by pbuf.c

#define NO_SYS 1
typedef unsigned char   u8_t;
typedef unsigned short u16_t;
typedef unsigned int   u32_t;
typedef signed   char  s8_t;
typedef signed   short s16_t;
typedef signed   int   s32_t;
/*
typedef unsigned int sys_sem_t;
typedef unsigned int sys_mbox_t;
typedef unsigned int sys_thread_t;
*/
typedef unsigned long mem_ptr_t;

//#ifdef __cplusplus
//extern "C" {
//#endif
void lwip_print(const char *format, ...);
void lwip_assert(const char *format, ...);
//#ifdef __cplusplus
//}
//#endif

#define LWIP_PLATFORM_ASSERT(message) do { lwip_assert(message); } while (0)
#define LWIP_PLATFORM_DIAG(message) do { lwip_print message; } while (0)
#define TCP_SND_BUF 2048

//#define LWIP_ARP 0 //disable ARP
//#define ARP_QUEUEING 0 //disable ARP
//#define IP_REASSEMBLY 0 //disable reassembly of incoming packets

// U16_F, S16_F, X16_F, U32_F, S32_F, X32_F, SZT_F
// usually defined to "hu", "d", "hx", "u", "d", "x", "uz" 
#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "lu"
#define S32_F "ld"
#define X32_F "lx"


/*
 * Enable/Disable features of lwip
 */
#define LWIP_DHCP 1
#define MEMP_NUM_SYS_TIMEOUT 5
#define ETHARP_TMR_INTERVAL 5000
#define LWIP_BROADCAST_PING 1
#define LWIP_MULTICAST_PING 1

#define LWIP_NETCONN 0
#define LWIP_SOCKET 0
//#define LWIP_ICMP 0
//#define TCP_LISTEN_BACKLOG 1
//#define TCP_DEFAULT_LISTEN_BACKLOG 1

#define MEM_ALIGNMENT 4

/*
 * DEBUG stuff
 */
//#define LWIP_DEBUG  1
//#define UDP_DEBUG   (LWIP_DBG_ON)
//#define TCP_DEBUG   (LWIP_DBG_ON)
//#define TCP_INPUT_DEBUG   (LWIP_DBG_ON)
//#define TCP_OUTPUT_DEBUG   (LWIP_DBG_ON)
//#define TCP_TCP_QLEN_DEBUG (LWIP_DBG_ON)
//#define TCP_CWND_DEBUG     (LWIP_DBG_ON)
//#define PBUF_DEBUG   (LWIP_DBG_ON)
//#define MEMP_DEBUG   (LWIP_DBG_ON)
//#define ETHARP_DEBUG (LWIP_DBG_ON)
//#define IP_DEBUG     (LWIP_DBG_ON)
//#define IP_REASS_DEBUG (LWIP_DBG_ON)
//#define DHCP_DEBUG   (LWIP_DBG_ON)
//#define ICMP_DEBUG   (LWIP_DBG_ON)
