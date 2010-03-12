
LIB_SRC= memb.c random.c etimer.c timer.c

UIP6_SRC= uip6.c tcpip.c uip-udp-packet.c uip-split.c \
	resolv.c hc.c uiplib.c uip-icmp6.c uip-nd6.c \
	uip-nd6-io.c uip-netif.c psock.c

include rime/Makefile.rime

6LOWPAN_SRC = sicslowpan.c

