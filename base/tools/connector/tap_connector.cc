/*
 * TAP Device Connector
 *
 * Copyright (C) 2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of the NOVA userland (NUL).
 *
 * NUL is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#include <linux/if_ether.h>
#include <linux/if_tun.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

//#define DEBUG

size_t const mtu_size = 16380;
size_t const buf_size = 0x400000;

enum Request
{
    NET_INIT    = 0x40001000,
    NET_WAIT    = 0x40001001,
};

enum Slot_state { EMPTY, FULL };

struct Buffer
{
    unsigned short sst;     // slot state
    unsigned short len;     // payload length
    char data[mtu_size];    // payload data
};

void barrier()
{
    asm volatile ("" : : : "memory");
}

/*
 * Perform a VM Exit to the VMM
 */
unsigned long vmm_call (Request eax, unsigned long ebx = 0, unsigned long ecx = 0, unsigned long edx = 0)
{
    asm volatile ("cpuid" : "+a" (eax), "+b" (ebx), "+c" (ecx), "+d" (edx) : : "memory");
    return edx;
}

/*
 * Create a new TAP device
 */
int if_add_tap (char **name)
{
    int fd;

    if ((fd = open ("/dev/net/tun", O_RDWR | O_NONBLOCK)) < 0) {
        perror ("open[/dev/net/tun]");
        return -1;
    }

    struct ifreq i;
    memset (&i, 0, sizeof (i));
    i.ifr_flags = IFF_TAP | IFF_NO_PI;

    if ((ioctl (fd, TUNSETIFF, &i)) < 0) {
        perror ("ioctl[TUNSETIFF]");
        close (fd);
        return -1;
    }

    if ((ioctl (fd, TUNSETNOCSUM, 1)) < 0) {
        perror ("ioctl[TUNSETNOCSUM]");
        close (fd);
        return -1;
    }

    if (name)
        *name = strdup (i.ifr_name);

    return fd;
}

/*
 * Get index for interface "ifname"
 */
int if_get_idx (int fd, char const *ifname)
{
    struct ifreq i;
    memset (&i, 0, sizeof (i));
    snprintf (i.ifr_name, IFNAMSIZ, "%s", ifname);

    if ((ioctl (fd, SIOCGIFINDEX, &i)) < 0) {
        perror ("ioctl[SIOCGIFINDEX]");
        return -1;
    }

    return i.ifr_ifindex;
}

/*
 * Bring up interface "ifname"
 */
bool if_set_up (int fd, char const *ifname)
{
    struct ifreq i;
    memset (&i, 0, sizeof (i));
    snprintf (i.ifr_name, IFNAMSIZ, "%s", ifname);

    if (ioctl (fd, SIOCGIFFLAGS, &i) < 0) {
        perror ("ioctl[SIOCGIFFLAGS]");
        return false;
    }

    i.ifr_flags |= IFF_UP | IFF_RUNNING;

    if (ioctl (fd, SIOCSIFFLAGS, &i) < 0) {
        perror ("ioctl[SIOCSIFFLAGS]");
        return false;
    }

    return true;
}

/*
 * Add interface with index ifidx to bridge "brdev"
 */
bool if_add_to_bridge (int fd, char const *brdev, int ifidx)
{
    struct ifreq i;
    memset (&i, 0, sizeof (i));
    snprintf (i.ifr_name, IFNAMSIZ, "%s", brdev);
    i.ifr_ifindex = ifidx;

    if (ioctl (fd, SIOCBRADDIF, &i) < 0) {
        perror ("ioctl[SIOCBRADDIF]");
        return false;
    }

    return true;
}

/*
 * Allocate buffer space of size "size"
 *
 * For MAP_HUGETLB to succeed, you need:
 * * hugetlbfs support in the Linux kernel
 * * sysctl -w vm.nr_hugepages=40 (or some other number)
 */
Buffer *map_buffer (size_t size)
{
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED | MAP_POPULATE;

#ifdef MAP_HUGETLB
    flags |= MAP_HUGETLB;
#endif

    void *addr;
    if ((addr = mmap (0, size, PROT_READ | PROT_WRITE, flags, -1, 0)) == MAP_FAILED) {
        perror ("mmap");
        return 0;
    }

    return static_cast<Buffer *>(addr);
}

int main (int argc, char *argv[])
{
    char *dev;
    int fd, idx, tap;

    if ((tap = if_add_tap (&dev)) < 0)
        return -1;

    if ((fd = socket (PF_INET, SOCK_DGRAM, 0)) < 0)
        return -1;

    if ((idx = if_get_idx (fd, dev)) < 0 || !if_set_up (fd, dev))
        return -1;

    if (argc > 1 && (!if_add_to_bridge (fd, argv[1], idx) || !if_set_up (fd, argv[1])))
        return -1;

    close (fd);

    Buffer *tap_buf, *vmm_buf;
    if (!(tap_buf = map_buffer (buf_size)) || !(vmm_buf = map_buffer (buf_size)))
        return -1;

    memset (tap_buf, 0, buf_size);
    memset (vmm_buf, 0, buf_size);

    printf ("Listening on Device:%s TAP:%p VMM:%p\n\n", dev, tap_buf, vmm_buf);

    if (!vmm_call (NET_INIT, 0, reinterpret_cast<unsigned long>(tap_buf), reinterpret_cast<unsigned long>(vmm_buf))) {
      puts ("Registering as donor application failed");
      return -1;
    }

    unsigned const slots = buf_size / sizeof (Buffer);
    unsigned tap_slot = 0, vmm_slot = 0;
    unsigned long inj_count = 0;

    for (;;) {

        bool block = true;

        // TAP => VMM
        if (*static_cast<volatile unsigned short *>(&tap_buf[tap_slot].sst) == EMPTY) {

            int len = read (tap, tap_buf[tap_slot].data, sizeof (tap_buf[tap_slot].data));

            if (len > 0) {

                tap_buf[tap_slot].len = len;

#ifdef DEBUG
                struct ethhdr *e = reinterpret_cast<struct ethhdr *>(tap_buf[tap_slot].data);
                printf ("TAP %04u: %02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x P:%04x L:%u I:%#lx\n",
                        tap_slot,
                        e->h_source[0], e->h_source[1], e->h_source[2],
                        e->h_source[3], e->h_source[4], e->h_source[5],
                        e->h_dest[0], e->h_dest[1], e->h_dest[2],
                        e->h_dest[3], e->h_dest[4], e->h_dest[5],
                        e->h_proto, tap_buf[tap_slot].len, inj_count);
#endif

                barrier();

                tap_buf[tap_slot].sst = FULL;

                tap_slot = (tap_slot + 1) % slots;

                block = false;

            } else if (len < 0 && errno != EAGAIN) {
                perror ("read");
                break;
            }
        }

        // VMM => TAP
        if (*static_cast<volatile unsigned short *>(&vmm_buf[vmm_slot].sst) == FULL) {

            int len = write (tap, vmm_buf[vmm_slot].data, vmm_buf[vmm_slot].len);

            if (len > 0) {

#ifdef DEBUG
                struct ethhdr *e = reinterpret_cast<struct ethhdr *>(vmm_buf[vmm_slot].data);
                printf ("VMM %04u: %02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x P:%04x L:%u\n",
                        vmm_slot,
                        e->h_source[0], e->h_source[1], e->h_source[2],
                        e->h_source[3], e->h_source[4], e->h_source[5],
                        e->h_dest[0], e->h_dest[1], e->h_dest[2],
                        e->h_dest[3], e->h_dest[4], e->h_dest[5],
                        e->h_proto, vmm_buf[vmm_slot].len);
#endif

                barrier();

                vmm_buf[vmm_slot].sst = EMPTY;

                vmm_slot = (vmm_slot + 1) % slots;

                block = false;

            } else if (len < 0 && errno != EAGAIN) {
                perror ("write");
                break;
            }
        }

        if (block)
            inj_count = vmm_call (NET_WAIT, 0, 0, inj_count);
    }

    close (tap);

    return 0;
}
