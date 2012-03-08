/** @file
 * Host Reboot.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include "nul/motherboard.h"


/**
 * Reboot the host machine.
 *
 * State: unstable
 * Features:  loop, keyboard, fastgate, PCI reset, ACPI reset methods (MMIO, IOIO, PCI0)
 * Missing: disable VMX, keyboard-wait-loop
 */
struct HostReboot : public StaticReceiver<HostReboot>
{
#include "host/simplehwioin.h"
#include "host/simplehwioout.h"
  enum {
    METHOD_KEYBOARD,
    METHOD_FASTGATE,
    METHOD_PCIRESET,
    METHOD_ACPI
  };
  unsigned _method;
  unsigned char _acpi_method;
  unsigned char _acpi_value;
  unsigned long long _acpi_address;

  /**
   * Request all the resources that are needed.
   */
  bool init(Motherboard &mb) {
    MessageHostOp msg0(MessageHostOp::OP_ALLOC_IOIO_REGION, ~0ul);
    switch (_method) {
    case METHOD_KEYBOARD: msg0.value =  0x6400; break;
    case METHOD_FASTGATE: msg0.value =  0x9200; break;
    case METHOD_PCIRESET: msg0.value = 0xcf900; break;
    case METHOD_ACPI:
      {
	MessageAcpi msg1("FACP");
	if (!mb.bus_acpi.send(msg1, true) || !msg1.table) return false;
	check1(false, msg1.len < 129, "FACP too small");
	check1(false,~msg1.table[113] & 0x4, "reset unsupported");
	check1(false, msg1.table[117] !=8, "register width invalid");
	check1(false, msg1.table[118] !=0, "register offset invalid");
	check1(false, msg1.table[119] > 1, "byte access needed");
	_acpi_method = msg1.table[116];
	_acpi_address= *reinterpret_cast<unsigned long long *>(msg1.table + 120);
	_acpi_value  =  msg1.table[128];
	switch (_acpi_method) {
	case 0: {
	  MessageHostOp msg2(MessageHostOp::OP_ALLOC_IOMEM, _acpi_address, 0x1);
	  check1(false, !mb.bus_hostop.send(msg2) || !msg2.ptr, "could not get MMIO region at %llx", _acpi_address);
	  _acpi_address = reinterpret_cast<unsigned long>(msg2.ptr);
	  return true;
	}
	case 1:
	  check1(false, _acpi_address >= 0x10000, "PIO out of range %llx", _acpi_address);
	  msg0.value = _acpi_address << 8;
	  break;
	case 2: msg0.value = 0xcf803; break;
	default:
	  check1(false, false, "wrong access type %x", _acpi_method);
	}
	break;
      }
    default:
      return false;
    }
    return msg0.value == ~0ul || mb.bus_hostop.send(msg0);
  }

  bool  receive(MessageConsole &msg) {
    if ((msg.type != MessageConsole::TYPE_RESET) || (msg.id != 0))  return false;

    Logging::printf("hr: resetting machine via method %d addr %llx value %x\n", _method, _acpi_address, _acpi_value);
    switch (_method) {
    case METHOD_KEYBOARD: outb(0xfe, 0x64);    break;
    case METHOD_FASTGATE: outb(0x01, 0x92);    break;
    case METHOD_PCIRESET:
      outb((inb(0xcf9) & ~4) | 0x02, 0xcf9);
      outb(0x06, 0xcf9);
      break;
    case METHOD_ACPI:
      switch (_acpi_method) {
      case 0:
	*reinterpret_cast<volatile unsigned char *>(_acpi_address) = _acpi_value;
	break;
      case 1: outb(_acpi_value, _acpi_address);             break;
      case 2:
	{
	  MessageHwIOOut msg(MessageIOOut::TYPE_OUTL, 0xcf8, 0x80000000);
	  msg.value |= (_acpi_address & 0x1f00000000ull) >> (32-11);
	  msg.value |= (_acpi_address & 0x70000) >> (16 - 8);
	  msg.value |= _acpi_address & 0x3c;
	  if (!_bus_hwioout.send(msg, true))
	    Logging::panic("hr: %s could not send to ioport %x\n", __PRETTY_FUNCTION__, msg.port);
	  outb(_acpi_value, 0xcfc | (_acpi_address & 0x3));
	}
	break;
      default:
	assert(0);
      }
      break;
    }
    return true;
  }

  HostReboot(DBus<MessageHwIOIn> &bus_hwioin, DBus<MessageHwIOOut> &bus_hwioout, unsigned method) : _bus_hwioin(bus_hwioin), _bus_hwioout(bus_hwioout), _method(method) {}
};


PARAM_HANDLER(hostreboot,
	      "hostreboot:type - provide the functionality to reboot the host.",
	      "Example: 'hostreboot:0' uses the keyboard to reboot the host.",
	      "type is one of [0:Keyboard, 1:FastGate, 2:PCI, 3:ACPI].")
{
  HostReboot *r = new HostReboot(mb.bus_hwioin, mb.bus_hwioout, argv[0]);
  if (r->init(mb)) {
    mb.bus_console.add(r, HostReboot::receive_static);
  }
}
// EOF
