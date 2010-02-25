/* -*- Mode: C++ -*-
 *
 * Minimalistic presentation tool. See README.org for details.
 *
 * All bugs added by Julian Stecklina <jsteckli@os.inf.tu-dresden.de>.
 */

#include "vmm/message.h"
#include "sigma0/console.h"
#include "models/keyboard.h"
#include <tinf.h>


#define LINES 1024
#define ROWS   768

extern char __freemem[];

struct pheader {
  char           magic[4];
  unsigned short width;
  unsigned short height;
  unsigned int   pages;
  unsigned int   offset[];
} NOVA_PACKED;

class Rocknshine : public NovaProgram, public ProgramConsole, GenericKeyboard
{

  char *  _vesa_console;
  VgaRegs _vesaregs;
  ConsoleModeInfo _modeinfo;

  pheader *_header;
  char *   _scratch;
  unsigned _scratch_size;


  void show_page(unsigned page)
  {
    unsigned *cur_vesa = reinterpret_cast<unsigned *>(_vesa_console);
    unsigned int dest_len = _scratch_size;

    unsigned char *compressed = _header->offset[page] + (unsigned char *)_header;
    unsigned size = _header->offset[page+1] - _header->offset[page];

    Logging::printf("Compressed page %d at %p (offset %x, 0x%x bytes).\n",
		    page, compressed, _header->offset[page], size);

    unsigned long long start_decompress = Cpu::rdtsc();
    int tinf_res = tinf_zlib_uncompress(_scratch, &dest_len,
					compressed, size);

    if (tinf_res != TINF_OK) {
      Logging::printf("tinf returned %d.\n", tinf_res);
      return;
    }

    unsigned long long start_display = Cpu::rdtsc();
    switch (_modeinfo.bpp) {
    case 24:                    // 24-bit mode
      memcpyl(cur_vesa, _scratch, _scratch_size / 4);
      break;
    case 32:                    // 32-bit mode
      for (unsigned i = 0; i < _scratch_size; i += 3*4) {

	// Load 4 packed pixels in 3 words
	unsigned *data = reinterpret_cast<unsigned *>(_scratch + i);
	unsigned p1 = data[0];
	unsigned p2 = data[1];
	unsigned p3 = data[2];

	cur_vesa[0] = p1 & 0xFFFFFF;
	cur_vesa[1] = (p1 >> 24) | ((p2 & 0xFFFF) << 8);
	cur_vesa[2] = (p2 >> 16) | ((p3 & 0xFF) << 16);
	cur_vesa[3] = p3 >> 8;
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
  int run(Hip *hip)
  {
    console_init("RS");
    unsigned res;
    if ((res = init(hip))) Logging::panic("init failed with %x", res);

    Logging::printf("rocknshine up and running\n");

    // Look for module
    {
      MessageHostOp msg(1, __freemem);
      if (!Sigma0Base::hostop(msg))
	Logging::panic("no module");

      _header = (pheader *) __freemem;
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

    while (Sigma0Base::console(msg))      {
      // we like to have a 24/32bit mode
      if (m.attr & 0x80 && m.bpp >= 24 && m.resolution[0] == LINES)

	// we prefer a 24bit mode
	if (!size || m.bpp < _modeinfo.bpp)
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
    unsigned input = 0;
    while (1) {
      if (last_page != page) show_page(page);
      last_page = page;

      MessageKeycode *kmsg = stdinconsumer->get_buffer();
      switch (kmsg->keycode & ~KBFLAG_NUM) {
      case KBCODE_SPACE:
      case KBCODE_DOWN:
      case KBCODE_RIGHT:
	page = (page+1) % _header->pages;
	break;
      case KBCODE_BSPACE:
      case KBCODE_UP:
      case KBCODE_LEFT:
	page = (page + _header->pages - 1) % _header->pages;
	break;
      case KBCODE_HOME:
	page = 0;
	break;
      case KBCODE_END:
	page = _header->pages - 1;
	break;
      case KBCODE_ENTER:
	if (input && input <= _header->pages) page = input - 1;
      case KBCODE_ESC:
	input = 0;
	break;
      case KBCODE_SCROLL:
	for (unsigned i=0; i < _header->pages; i++)
	  {
	    page = (page + 1) % _header->pages;
	    show_page(page);
	  }
	break;
      default:
	{
	  unsigned num;
	  if ((num = is_numeric_key(kmsg->keycode, 0)))  input = input*10 + num;
	}
      }

      stdinconsumer->free_buffer();
    }
  }
};

ASMFUNCS(Rocknshine, NovaProgram);
