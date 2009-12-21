#include <sys/console.h>
#include <vmm/motherboard.h>
#include <sigma0/sigma0.h>
#include <stdint.h>

void print_screen1(uint16_t *dst);

class Tutor : public NovaProgram,
	      public ProgramConsole
{
  const char *debug_getname() { return "Tutor"; };

public:
  static void exit(unsigned long status)
  {
    Logging::printf("%s(%lx)\n", __func__, status);
  }

  void run(Hip *hip)
  {
    console_init("TUT");

    init(hip);

    print_screen1(_console_data.screen_address);

    block_forever();

  }
};

ASMFUNCS(Tutor);

// EOF
