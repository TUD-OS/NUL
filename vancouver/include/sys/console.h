#include "vmm/message.h"
#include "host/screen.h"
#include "sigma0/sigma0.h"

/**
 * A helper class that implements a vga console and forwards message to sigma0.
 */
class ProgramConsole
{
 protected:
  struct console_data {
    VgaRegs *regs;
    unsigned short *screen_address;
    unsigned index;
    char buffer[100];
  };


  static void putc(void *data, long value)
  {
    struct console_data *d = reinterpret_cast<struct console_data *>(data);
    if (d->screen_address)  Screen::vga_putc(0xf00 | value, d->screen_address, d->regs->cursor_pos);
    if (value == '\n' || d->index == sizeof(d->buffer) - 1)
      {
	d->buffer[d->index] = 0;
	Sigma0Base::puts(d->buffer);
	d->index = 0;
	if (value != '\n')
	  {
	    d->buffer[d->index++] = '|';
	    d->buffer[d->index++] = ' ';
	    d->buffer[d->index++] = value;
	  }
      }
    else
      d->buffer[d->index++] = value;
  }


  VgaRegs             _vga_regs;
  struct console_data _console_data;

  void console_init(const char *name)
  {
    char *vga_console = reinterpret_cast<char *>(memalign(0x1000, 0x1000));
    _console_data.screen_address = reinterpret_cast<unsigned short *>(vga_console);
    _console_data.regs = &_vga_regs;
    Logging::init(putc, &_console_data);
    _vga_regs.cursor_pos = 24*80*2;
    _vga_regs.offset = 0;
    _vga_regs.cursor_style = 0x2000;

    MessageConsole msg(name, vga_console, 0x1000, &_vga_regs);
    Sigma0Base::console(msg);
  }
};
