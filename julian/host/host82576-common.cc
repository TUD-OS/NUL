// -*- Mode: C++
// Common routines for drivers

#include <host/jsdriver.h>

void
PciDriver::spin(unsigned micros)
{
  timevalue done = _clock->abstime(micros, 1000000);
  while (_clock->time() < done)
    Cpu::pause();
}

bool
PciDriver::wait(volatile uint32 &reg, uint32 mask, uint32 value,
		unsigned timeout_micros)
{
  timevalue timeout = _clock->abstime(timeout_micros, 1000000);
  
  while ((reg & mask) != value) {
    Cpu::pause();
    if (_clock->time() >= timeout)
      return false;
  }
  return true;
}

void
PciDriver::msg(unsigned level, const char *msg, ...)
{
  if ((level & _msg_level) != 0) {
    va_list ap;
    va_start(ap, msg);
    Logging::printf("82576 %02x: ", _bdf & 0xFF);
    Logging::vprintf(msg, ap);
    va_end(ap);
  }
}

// EOF
