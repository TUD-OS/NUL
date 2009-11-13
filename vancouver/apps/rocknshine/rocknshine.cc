/* -*- Mode: C++ -*-
 *
 * Minimalistic presentation tool. See README.org for details.
 *
 * All bugs added by Julian Stecklina <jsteckli@os.inf.tu-dresden.de>.
 */

#include "sys/console.h"
#include "sys/syscalls.h"
#include "driver/logging.h"
#include "vmm/message.h"
#include "vmm/timer.h"
#include "vmm/math.h"
#include "vmm/cpu.h"
#include "sigma0/sigma0.h"

#include <tinf/tinf.h>

#define LINES 1024
#define ROWS   768

extern char __module_start[];
extern char __module_end[];

struct pheader {
  char           magic[4];
  unsigned short width;
  unsigned short height;
  unsigned int   pages;
  unsigned int   offset[];
} NOVA_PACKED;

class Rocknshine : public NovaProgram, public ProgramConsole
{

  char *  _vesa_console;
  VgaRegs _vesaregs;
  ConsoleModeInfo _modeinfo;

  pheader *_header;
  char *   _scratch;
  unsigned _scratch_size;

  static unsigned bswap(unsigned x)
  {
    unsigned out;
    asm ("bswap %0\n" : "=r" (out) : "0" (x));
    return out;
  }

  static unsigned xyz_to_zyx(unsigned x)
  {
    return bswap(x) >> 8;
  }

  void show_page(unsigned page)
  {
    unsigned *cur_vesa = reinterpret_cast<unsigned *>(_vesa_console);
    unsigned int dest_len = _scratch_size;

    unsigned long long start_decompress = Cpu::rdtsc();
    unsigned char *compressed = _header->offset[page] + (unsigned char *)_header;
    unsigned size = _header->offset[page+1] - _header->offset[page];

    Logging::printf("Compressed page %d at %p (offset %x, 0x%x bytes).\n",
                    page, compressed, _header->offset[page], size);
    Logging::printf("CRC32 of compressed page: %d\n", tinf_crc32(compressed, size));
    memset(_scratch, 0, _scratch_size);
    int tinf_res = tinf_zlib_uncompress(_scratch, &dest_len,
                                        compressed, size);

    if (tinf_res != TINF_OK) {
      Logging::printf("tinf returned %d.\n", tinf_res);
      return;
    }

    unsigned long long start_display = Cpu::rdtsc();

    Logging::printf("%u bytes -> %u bytes.\n",
                    size, dest_len);
    switch (_modeinfo.bpp) {
    case 24:                    // 24-bit mode
      for (unsigned i = 0; i < _scratch_size; i += 3*4) {
	// Load 4 packed pixels in 3 words
        unsigned *data = reinterpret_cast<unsigned *>(_scratch + i);
        unsigned p1 = data[0];
        unsigned p2 = data[1];
        unsigned p3 = data[2];

        cur_vesa[0] = bswap(p1) >> 8 | ((p2 >> 8) << 24);
        cur_vesa[1] = (p2 & 0xFF0000FF) | ((p3 & 0xFF) << 16) | ((p1 >> 24) << 8);
        cur_vesa[2] = bswap(p3) << 8 | ((p2 >> 16) & 0xFF);
        cur_vesa   += 3;
      }
      
      break;
    case 32:                    // 32-bit mode
      for (unsigned i = 0; i < _scratch_size; i += 3*4) {
        // Load 4 packed pixels in 3 words
        unsigned *data = reinterpret_cast<unsigned *>(_scratch + i);
        unsigned p1 = data[0];
        unsigned p2 = data[1];
        unsigned p3 = data[2];

        unsigned u1 = p1 & 0xFFFFFF;
        unsigned u2 = (p1 >> 24) | ((p2 & 0xFFFF) << 8);
        unsigned u3 = (p2 >> 16) | ((p3 & 0xFF) << 16);
        unsigned u4 = p3 >> 8;
        
        cur_vesa[0] = xyz_to_zyx(u1);
        cur_vesa[1] = xyz_to_zyx(u2);
        cur_vesa[2] = xyz_to_zyx(u3);
        cur_vesa[3] = xyz_to_zyx(u4);
        cur_vesa   += 4;
      }
      break;
    default:
      // XXX ?
      {}
    };

    unsigned long long end_cycles = Cpu::rdtsc();
    Logging::printf("Decompression: %u cycles\n", (unsigned)(start_display - start_decompress));
    Logging::printf("Display:       %u cycles\n", (unsigned)(end_cycles - start_display));
  };

public:
  static void exit(unsigned long status)
  {
    Logging::printf("%s(%lx)\n", __func__, status);
  }

  int run(Hip *hip)
  {
    console_init("RS");
    unsigned res;
    if ((res = init(hip))) Logging::panic("init failed with %x", res);

    Logging::printf("rocknshine up and running\n");

    // Look for module
    {
      MessageHostOp msg(1, __module_start);
      if (!Sigma0Base::hostop(msg))
	Logging::panic("no module");

      _header = (pheader *)__module_start;
      if (memcmp(_header->magic, "PRE0", sizeof(_header->magic)) != 0)
	Logging::panic("invalid module");

      Logging::printf("Loaded presentation: (%d, %d), %d page%s.\n",
		      _header->width, _header->height, _header->pages,
		      (_header->pages == 1) ? "" : "s");
    }

    // Initialize tiny zlib library.
    tinf_init();
    Logging::printf("tinf library initialized.\n");

    unsigned size = 0;
    unsigned mode = ~0;
    ConsoleModeInfo m;
    MessageConsole msg(0, &m);
    // XXX Scan twice to prefer 4-bytes-per-pixel modes
    while (Sigma0Base::console(msg))      {
      // Logging::printf("rocknshine: %x %dx%d-%d sc %x\n", 
      //   	      msg.index, m.resolution[0], m.resolution[1], m.bpp, m.bytes_per_scanline);
      // we like to have the 24/32bit mode with 1024
      if (!m.textmode && m.bpp >= 24 && m.resolution[0] == LINES)
        {
          size = m.resolution[1] * m.bytes_per_scanline;
          mode = msg.index;
          _modeinfo = m;
        }
      msg.index++;
    }

    if (mode == ~0u) Logging::panic("have not found any 32bit graphic mode");

    _scratch_size = size;
    Logging::printf("RS: _scratch_size = %x\n", _scratch_size);
    _scratch      = reinterpret_cast<char *>(malloc(size));
    _vesa_console = reinterpret_cast<char *>(memalign(0x1000, size));
    Logging::printf("RS: use %x %dx%d-%d %p size %x sc %x\n", 
		    mode, _modeinfo.resolution[0], _modeinfo.resolution[1], _modeinfo.bpp, _vesa_console, size, _modeinfo.bytes_per_scanline);

    MessageConsole msg2("RS2", _vesa_console, size, &_vesaregs);
    check(!Sigma0Base::console(msg2));
    _vesaregs.mode = mode;

    // Get keyboard
    Logging::printf("Getting keyboard\n");
    
    StdinConsumer *stdinconsumer = new StdinConsumer(_cap_free++);
    Sigma0Base::request_stdin(stdinconsumer, stdinconsumer->sm());
	 
    msg.type = MessageConsole::TYPE_SWITCH_VIEW;
    msg.view = 1;
    Sigma0Base::console(msg);

    unsigned last_page = 1;     // Force redraw
    unsigned page = 0;
    while (1) {
      if (last_page != page)
        show_page(page);
      last_page = page;

      MessageKeycode *kmsg = stdinconsumer->get_buffer();
      Logging::printf("keycode = %x\n", kmsg->keycode);
      switch (kmsg->keycode) {
      case 0x829:               // Space down
      case 0xa72:               // Arrow Down down
      case 0xa74:               // Arrow Right down
        page = (page+1) % _header->pages;
        break;
      case 0x866:               // Backspace down
      case 0xa75:               // Arrow Down down
      case 0xa6b:               // Arrow Right down
        page = (page + _header->pages - 1) % _header->pages;
        break;
      case 0x816:               // 1 down
        page = 0;
        break;
      default:
        // Unknown character
        {}
      }

      stdinconsumer->free_buffer();
    }
  }
};

ASMFUNCS(Rocknshine);
