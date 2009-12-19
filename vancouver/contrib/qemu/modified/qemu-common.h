#pragma once
#include <stdarg.h>
#include "inttypes.h"
#include "bswap.h"

#define	EINVAL	22
#define NULL    0
#define device_init(X)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct iovec {
  void *iov_base;
  size_t iov_len;
};

/* stdlib+string+stdio */
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);

void *qemu_mallocz(size_t size);
void *qemu_free(void *p);

/* CPU  */
typedef uint32_t CPUReadMemoryFunc(void *opaque, target_phys_addr_t addr);
typedef void CPUWriteMemoryFunc(void *opaque, target_phys_addr_t addr, uint32_t value);
typedef int (*DMA_transfer_handler) (void *opaque, int nchan, int pos, int size);

/* PTR typedefs */
typedef struct FILE FILE;
typedef struct NICInfo NICInfo;
typedef struct BlockDriverState BlockDriverState;
typedef struct CharDriverState CharDriverState;
typedef struct DeviceState DeviceState;
typedef struct DisplayState DisplayState;
typedef struct Monitor Monitor;
typedef struct VLANState VLANState;
typedef struct QEMUFile QEMUFile;
typedef struct PCIBus PCIBus;
typedef struct PCIDevice PCIDevice;
typedef struct IRQState *qemu_irq;

/* debug printf */
void _ZN7Logging6printfEPKcz(char *, ...);
#define printf _ZN7Logging6printfEPKcz
