// -*- Mode: C++ -*-

#include <vmm/message.h>
#include <vmm/motherboard.h>
#include <cstdint>

template <class T>
class ListEntry {

public:
  T *next;
};

class NubusManager {
protected:
  DBus<MessagePciConfig> &_pci_extcfg;

  class Device;

  class Bus : ListEntry<Bus> {
  protected:
    NubusManager &_manager;
    uint8_t _no;
    Device *_root;

    Device *_devices;
    Bus    *_buses;

  public:
    uint32_t
    read_cfg(uint8_t df, uint16_t reg) {
      MessagePciConfig msg(_no << 8 | df, reg);

      if (!_manager._pci_extcfg.send(msg))
	return ~0UL;
      else
	return msg.value;
    }

    Bus(NubusManager &manager, uint8_t no, Device *root)
      : _manager(manager), _no(no), _root(root), _devices(NULL), _buses(NULL) {
      Logging::printf("Nubus discovered bus %u.\n", no);
      
      for (unsigned df = 0; df < 32*8; df++) {
	uint32_t vendor = read_cfg(df, 0);
	if (vendor != ~0UL)
	  Logging::printf("Bus[%u] = %08x\n", df, vendor);
      }

    }
  };

  class Device : ListEntry<Device> {
  protected:
    Bus &_bus;
    uint8_t _df;

  public:
    Device(Bus &bus, uint8_t df) 
      : _bus(bus), _df(df) {

    }
  };

  Bus _root_bus;

public:

  NubusManager(DBus<MessagePciConfig> &pci_extcfg)
    : _pci_extcfg(pci_extcfg),
      _root_bus(*this, 0, NULL)
  {
    Logging::printf("Nubus initialized.\n");
  }
};


PARAM(nubus,
      {
	new NubusManager(mb.bus_hwpcicfg);
      },
      "nubus - PCI bus manager");

// EOF
