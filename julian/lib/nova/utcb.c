/* -*- Mode: C -*- */

#include <nova/utcb.h>
#include <nova/cpu.h>

NOVA_EXTERN_C uint32_t
utcb_add_mappings(Utcb *utcb, bool exception, uint32_t addr, uint32_t size, uint32_t hotspot, unsigned rights)
{
  while (size > 0)
    {
      unsigned ms = minshift(addr | hotspot, size, 31);
      //Logging::printf("add_mappings(%lx, %lx, %lx) ms %x\n", addr, size, hotspot, ms);
      //assert(ms >= MAPMINSHIFT);
      uint32_t *item = (exception ? utcb->items : 
			(utcb->msg + mtd_untyped(utcb->head.mtr))) + mtd_typed(utcb->head.mtr)*2;
      if (NOVA_CAST(Utcb *, item) >= utcb+1 || mtd_typed(utcb->head.mtr) >= 255) return size;
      item[0] = hotspot;
      item[1] = addr | ((ms-MAPMINSHIFT) << 7) | rights;
      utcb->head.mtr =
	untyped_words(mtd_untyped(utcb->head.mtr)) |
	typed_words(mtd_typed(utcb->head.mtr) + 1);

      unsigned long mapsize = 1 << ms;
      size    -= mapsize;
      addr    += mapsize;
      hotspot += mapsize;
    }
  return size;
}

/* EOF */
