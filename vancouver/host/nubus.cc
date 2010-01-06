// -*- Mode: C++ -*-

#include <vmm/message.h>
#include <vmm/motherboard.h>
#include <cstdint>

template <class T>
class ListEntry {

public:
  T *_next;
  explicit ListEntry(T *next = NULL) : _next(next) {}
};

enum {
  CLASS_PCI_TO_PCI_BRIDGE = 0x0604,
  CAP_PCI_EXPRESS         = 0x10,

  EXTCAP_ARI              = 0x000E,
  EXTCAP_SRIOV            = 0x0010,

  SRIOV_REG_CONTROL       = 0x8,
  SRIOV_VF_ENABLE         = 1,
  SRIOV_ARI               = (1<<4), // Only in function 0.
};

class NubusManager {
protected:
  DBus<MessagePciConfig> &_pci_extcfg;

  class Bus;

  class Device : public ListEntry<Device> {
  protected:
    Bus &_bus;
    uint8_t _df;
    uint32_t _vendor;
    uint32_t _dclass;
    uint8_t _header_type;

  public:
    Bus &bus() const { return _bus; }
    uint32_t read_cfg(uint16_t reg) { return _bus.read_cfg(_df, reg); }
    bool    write_cfg(uint16_t reg, uint32_t val) { return _bus.write_cfg(_df, reg, val); }

    bool multifunction() const { return (_header_type & (1<<7)) != 0; }

    bool ari_capable() {
      if (_dclass == CLASS_PCI_TO_PCI_BRIDGE) {
	// Bridges are ARI capable if their Device Capabilities 2
	// register has the ARI bit set.

	uint8_t express_cap = find_cap(CAP_PCI_EXPRESS);
	if (express_cap == 0) return false;
	
	uint32_t devcap2 = read_cfg(express_cap + 0x24);
	return (devcap2 & (1<<5)) != 0;
      } else {
	// Normal devices need an ARI capability to be ARI capable.
	return find_extcap(EXTCAP_ARI) != 0;
      }
    }

    bool ari_enabled() {
      uint8_t express_cap = find_cap(CAP_PCI_EXPRESS);
      return express_cap ? ((read_cfg(express_cap + 0x28) & (1<<5)) != 0) : false;
    }

    bool ari_enable() {
      if (_dclass != CLASS_PCI_TO_PCI_BRIDGE) return false;
      uint8_t express_cap = find_cap(CAP_PCI_EXPRESS);
      if (express_cap == 0) return false;
      
      uint32_t devctrl2 = read_cfg(express_cap + 0x28);
      write_cfg(express_cap + 0x28,  devctrl2 | (1<<5));
      return ari_enabled();
    }

    uint8_t ari_next_function() {
      uint16_t ari_cap = find_extcap(EXTCAP_ARI);
      if (ari_cap == 0) return 0;
      return (read_cfg(ari_cap + 4) >> 8) & 0xFF;
    }

    uint8_t find_cap(uint8_t id) {
      if ((read_cfg(4) >> 16) & 0x10 /* Capabilities supported? */) {
	for (unsigned offset = read_cfg(0x34) & 0xFC;
	     offset != 0; offset = (read_cfg(offset) >> 8) & 0xFC) {
	  if ((read_cfg(offset) & 0xFF) == id)
	    return offset;
	}
      }
      return 0;
    }

    uint16_t find_extcap(uint16_t id) {
      if (!find_cap(CAP_PCI_EXPRESS)) return 0;
      if (~0UL == read_cfg(0x100)) return 0;

      uint32_t header;
      uint16_t offset;
      for (offset = 0x100, header = read_cfg(offset);
	   offset != 0;
	   offset = header>>20, header = read_cfg(offset)) {
	if ((header & 0xFFFF) == id)
	  return offset;
      }

      return 0;			// XXX
    }

    Device(Bus &bus, uint8_t df)
      : _bus(bus), _df(df) {

      _vendor = read_cfg(0);
      _dclass = read_cfg(8) >> 16;

      _header_type = (read_cfg(12) >> 16) & 0xFF;
      uint8_t config_layout = _header_type & 0x3F;

      Logging::printf("dev[%x:%02x.%x] vendor %08x class %04x\n", _bus.no(), _df>>3, _df&7, _vendor, _dclass);

      uint16_t sriov_cap = find_extcap(EXTCAP_SRIOV);
      if (sriov_cap != 0) {
	uint32_t ctrl = read_cfg(sriov_cap + SRIOV_REG_CONTROL);
	if ((ctrl & SRIOV_VF_ENABLE) != 0) {
	  Logging::printf("SR-IOV already enabled. This is bad.");
	  goto sriov_skip;
	}
	  
	if (df == 0) {
	  Logging::printf("Adapting ARI status on SR-IOV capable device.\n");
	  write_cfg(sriov_cap + SRIOV_REG_CONTROL,
		    (ctrl & ~SRIOV_VF_ENABLE) | (_bus.ari_enabled() ? SRIOV_ARI : 0));
	}
      }
      sriov_skip:

      switch (_dclass) {
      case CLASS_PCI_TO_PCI_BRIDGE: {
	if (config_layout != 1) {
	  Logging::printf("Invalid layout %x for PCI-to-PCI bridge.\n", config_layout);
	  break;
	}
	new Bus(_bus, *this);
	break;
      }
      default:
	// XXX Look for driver.
	{}
      }
    }
  };

  class Bus : public ListEntry<Bus> {
  protected:
    NubusManager &_manager;
    uint8_t _no;
    Device *_bridge;

    Device *_devices;
    Bus    *_buses;

    void
    add_bus(Bus *bus) {
      bus->_next = _buses;
      _buses = bus;
    }

    Device *
    add_device(Device *device) {
      device->_next = _devices;
      _devices = device;
      return device;
    }
    
    bool device_exists(unsigned df) { return read_cfg(df, 0) != ~0UL; };

    void
    discover_devices() {
      if (ari_enabled()) {
	Logging::printf("ARI-aware device discovery for bus %u.\n", _no);
	// ARI-aware discovery
	uint16_t next = 0;
	do {
	  if (device_exists(next)) {
	    Device *dev = add_device(new Device(*this, next));
	    next = dev->ari_next_function();
	  } else {
	    Logging::printf("ARI-aware discovery b0rken or weird PCI topology.\n");
	    break;
	  }
	} while (next != 0);
      } else {
	// Non-ARI (i.e. normal) discovery
	for (unsigned df = 0; df < 32*8; df += 8) {
	  if (device_exists(df)) {
	    add_device(new Device(*this, df));
	    if (_devices->multifunction())
	      for (unsigned f = 1; f < 8; f++)
		if (device_exists(df | f))
		  add_device(new Device(*this, df | f));
	  }
	}
      }
    }

  public:
    uint8_t no() const { return _no; }

    uint32_t read_cfg(uint8_t df, uint16_t reg) {
      MessagePciConfig msg(_no << 8 | df, reg);
      return (!_manager._pci_extcfg.send(msg)) ? ~0UL : msg.value;
    }

    bool write_cfg(uint8_t df, uint16_t reg, uint32_t val) {
      MessagePciConfig msg(_no << 8 | df, reg, val);
      return _manager._pci_extcfg.send(msg);
    }

    bool ari_enable()  { return _bridge && (_bridge->ari_enable()); }
    bool ari_enabled() { return _bridge && (_bridge->ari_enabled()); }

    bool ari_capable() {
      // All devices and the bridge must be ARI capable.
      if (_bridge && _bridge->ari_capable()) {
	for (Device *dev = _devices; dev != NULL; dev = dev->_next) {
	  if (!dev->ari_capable()) {
	    return false;
	  }
	}
	return true;
      }
      return false;
    }
    
    explicit Bus(NubusManager &manager)
      : _manager(manager), _no(0), _bridge(NULL), _devices(NULL), _buses(NULL) {
      discover_devices();
    }

    Bus(Bus &parent, Device &bridge)
      : _manager(parent._manager), _bridge(&bridge), _devices(NULL), _buses(NULL) {
      _no = (_bridge->read_cfg(0x18) >> 8) & 0xFF;
      parent.add_bus(this);

      // if (_no == 1) {
      // 	Logging::printf("Force enable ARI for bus %u.\n", _no);
      // 	ari_enable();
      // }
      
      discover_devices();

      bool all_ari_capable = false;
      if (bridge.ari_capable()) {
      }
      if (all_ari_capable)
	Logging::printf("Bus %u is ARI-capable!\n", _no);
    }
    
  };

  Bus _root_bus;

public:

  NubusManager(DBus<MessagePciConfig> &pci_extcfg)
    : _pci_extcfg(pci_extcfg),
      _root_bus(*this)
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
