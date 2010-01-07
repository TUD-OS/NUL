// -*- Mode: C++ -*-

#include <vmm/message.h>
#include <vmm/motherboard.h>
#include <cstdint>
#include <cassert>

template <class T>
class ListEntry {

private:
  T *_prev;
  T *_next;

public:
  T *prev() const { return _prev; }
  T *next() const { return _next; }

  T *first() {
    T *first;
    for (first = (T *)this; first->_prev != NULL; first = first->_prev);
    return prev;
  }

  T *last() {
    T *last;
    for (last = (T *)this; last->_next != NULL; last = last->_next);
    return last;
  }

  void append(T *i) {
    T *la = last();
    la->_next = i;
    i->_prev = la;
  }

  explicit ListEntry() : _prev(NULL), _next(NULL) {}
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
  DBus<MessagePciConfig> &_pcicfg;
  Clock *_clock;

  class Bus;

  class Device : public ListEntry<Device> {
  protected:
    Bus &_bus;
    uint8_t _df;
    uint32_t _vendor;		// First word of ConfigSpace
    uint32_t _dclass;
    uint8_t _header_type;

    // Physical function (only set for VFs)
    Device *_pf;

  public:
    Bus &bus() const { return _bus; }
    uint32_t vendor() const { return _vendor; }
    uint32_t read_cfg(uint16_t reg) { return _bus.read_cfg(_df, reg); }
    bool write_cfg(uint16_t reg, uint32_t val) { return _bus.write_cfg(_df, reg, val); }
    bool is_vf() const { return _pf != NULL; }
    Device  *pf() const { return _pf; }
    uint16_t bdf() const { return (_bus.no()<<8) | _df; }

    uint8_t find_cap(uint8_t id);
    uint16_t find_extcap(uint16_t id);

    bool multifunction() const { return (_header_type & (1<<7)) != 0; }

    bool ari_capable();

    bool ari_bridge_enabled() {
      uint8_t express_cap = find_cap(CAP_PCI_EXPRESS);
      return express_cap ? ((read_cfg(express_cap + 0x28) & (1<<5)) != 0) : false;
    }

    bool ari_bridge_enable() {
      if (_dclass != CLASS_PCI_TO_PCI_BRIDGE) return false;
      uint8_t express_cap = find_cap(CAP_PCI_EXPRESS);
      if (express_cap == 0) return false;
      
      uint32_t devctrl2 = read_cfg(express_cap + 0x28);
      write_cfg(express_cap + 0x28,  devctrl2 | (1<<5));

      assert(ari_bridge_enabled());
      return true;
    }

    uint8_t ari_next_function() {
      uint16_t ari_cap = find_extcap(EXTCAP_ARI);
      if (ari_cap == 0) return 0;
      return (read_cfg(ari_cap + 4) >> 8) & 0xFF;
    }

    uint16_t sriov_total_vfs() {
      uint16_t sriov_cap = find_extcap(EXTCAP_SRIOV);
      if (sriov_cap == 0) return 0;
      return read_cfg(sriov_cap + 0xC) >> 16;
    }

    uint16_t sriov_device_id() {
      uint16_t sriov_cap = find_extcap(EXTCAP_SRIOV);
      if (sriov_cap == 0) return 0;
      return read_cfg(sriov_cap + 0x18) >> 16;
    }

    bool sriov_enable(uint16_t vfs_to_enable);

    Device(Bus &bus, uint8_t df, Device *pf = NULL)
      : _bus(bus), _df(df), _pf(pf) {

      _vendor = !is_vf() ? read_cfg(0) : ((this->pf()->vendor() & 0xFFFF) | 
					  (this->pf()->sriov_device_id()<<16)) ;
      _dclass = read_cfg(8) >> 16;
      if (is_vf()) assert(_dclass == pf->_dclass);

      _header_type = (read_cfg(12) >> 16) & 0xFF;
      uint8_t config_layout = _header_type & 0x3F;

      Logging::printf("dev[%x:%02x.%x] vendor %08x class %04x%s\n", _bus.no(), _df>>3, _df&7,
		      _vendor, _dclass, is_vf() ? " VF" : "");

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

    bool device_exists(unsigned df) { return read_cfg(df, 0) != ~0UL; };
    void discover_devices();

  public:
    uint8_t no() const { return _no; }
    NubusManager &manager() const { return _manager; };

    Bus *add_bus(Bus *bus) {
      if (!_buses) _buses = bus; else _buses->append(bus);
      return bus;
    }

    Device *add_device(Device *device) {
      if (!_devices) _devices = device; else _devices->append(device);
      return device;
    }
    
    uint32_t read_cfg(uint8_t df, uint16_t reg) {
      MessagePciConfig msg(_no << 8 | df, reg);
      return (!_manager._pcicfg.send(msg)) ? ~0UL : msg.value;
    }

    bool write_cfg(uint8_t df, uint16_t reg, uint32_t val) {
      MessagePciConfig msg(_no << 8 | df, reg, val);
      return _manager._pcicfg.send(msg);
    }

    bool ari_enable()  { return _bridge && (_bridge->ari_bridge_enable()); }
    bool ari_enabled() { return _bridge && (_bridge->ari_bridge_enabled()); }

    bool ari_capable();
    
    // Constructor for the root bus. Only called by the manager itself.
    explicit Bus(NubusManager &manager)
      : _manager(manager), _no(0), _bridge(NULL), _devices(NULL), _buses(NULL) {
      discover_devices();
    }

    Bus(Bus &parent, Device &bridge)
      : _manager(parent._manager), _bridge(&bridge), _devices(NULL), _buses(NULL) {
      _no = (_bridge->read_cfg(0x18) >> 8) & 0xFF;
      parent.add_bus(this);

      discover_devices();

      // We can only check, if we can enable ARI after devices on this
      // bus are discovered. They all have to support ARI.
      if (ari_capable()) {
	Logging::printf("bus[%x] Enabling ARI.\n", _no);
	assert(ari_enable());
	
	// It is important to enable SR-IOV in ascending order
	// (regarding functions). The SR-IOV capability of dependent
	// PFs might otherwise change, when we don't look anymore.

	// The list is changed while we traverse it, because
	// sriov_enable() adds the VFs. Should be no problem though,
	// because sriov_enable is a no-op for them.
	for (Device *dev = _devices; dev != NULL; dev = dev->next())
	  dev->sriov_enable(dev->sriov_total_vfs());

	// XXX Perhaps enabling ARI brought us some new
	// devices. Continue device discovery.
      }
    }
    
  };

  Bus _root_bus;

public:

  // XXX This could be much nicer if device initialization would run
  // in a separate thread that can sleep.
  void spin(unsigned ms) {
    timevalue timeout = _clock->abstime(ms, 1000);
    while (_clock->time() < timeout) {
    }
  }

  NubusManager(DBus<MessagePciConfig> &pcicfg, Clock *clock)
    : _pcicfg(pcicfg), _clock(clock), _root_bus(*this)
  {
    Logging::printf("Nubus initialized.\n");
  }
};

void NubusManager::Bus::discover_devices()
{
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

bool NubusManager::Bus::ari_capable()
{
  // All devices and the bridge must be ARI capable.
  if (_bridge && _bridge->ari_capable()) {
    for (Device *dev = _devices; dev != NULL; dev = dev->next())
      if (!dev->ari_capable())
	return false;
    return true;
  }
  return false;
}

bool NubusManager::Device::ari_capable()
{
  assert(!is_vf());
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

uint8_t NubusManager::Device::find_cap(uint8_t id)
{
  if ((read_cfg(4) >> 16) & 0x10 /* Capabilities supported? */) {
    for (unsigned offset = read_cfg(0x34) & 0xFC;
	 offset != 0; offset = (read_cfg(offset) >> 8) & 0xFC) {
      if ((read_cfg(offset) & 0xFF) == id)
	return offset;
    }
  }

  return 0;
}

uint16_t NubusManager::Device::find_extcap(uint16_t id)
{
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

bool NubusManager::Device::sriov_enable(uint16_t vfs_to_enable)
{
  uint16_t sriov_cap = find_extcap(EXTCAP_SRIOV);
  if (sriov_cap == 0) return false;

  uint32_t ctrl = read_cfg(sriov_cap + SRIOV_REG_CONTROL);
  if ((ctrl & SRIOV_VF_ENABLE) != 0) {
    Logging::printf("SR-IOV already enabled. This is bad.");
    return false;
  }
	  
  if (_df == 0) {
    // This is only needed for PF0 according to SR-IOV spec.
    Logging::printf("dev[%x:%02x.%x] Adapting ARI status on SR-IOV capable device.\n",
		    _bus.no(), _df>>3, _df&3);
    write_cfg(sriov_cap + SRIOV_REG_CONTROL,
	      (ctrl & ~SRIOV_ARI) | (_bus.ari_enabled() ? SRIOV_ARI : 0));
  }

  // for (unsigned off = 0; off < 0x24; off += 4) {
  //   Logging::printf("SR-IOV[%02x] %08x\n", off, read_cfg(sriov_cap + off));
  // }

  write_cfg(sriov_cap + 0x10, (read_cfg(sriov_cap + 0x10) & ~0xFFFF) | vfs_to_enable);
  write_cfg(sriov_cap + SRIOV_REG_CONTROL, SRIOV_VF_ENABLE | read_cfg(sriov_cap + SRIOV_REG_CONTROL));

  Logging::printf("dev[%x:%02x.%x] Enabled %u VFs. Wait for them to settle down.\n",
		  _bus.no(), _df>>3, _df&3, vfs_to_enable);
  _bus.manager().spin(200 /* ms */);

  // Add VFs to bus.
  unsigned vf_offset = read_cfg(sriov_cap + 0x14);
  unsigned vf_stride = vf_offset >> 16;
  vf_offset &= 0xFFFF;

  for (unsigned cur_vf = 0; cur_vf < vfs_to_enable; cur_vf++) {
    uint16_t vf_bdf = bdf() + vf_offset + vf_stride*cur_vf;
    if (vf_bdf>>8 != _bus.no()) {
      Logging::printf("VF %d not on our bus. Crap.\n", cur_vf);
      break;
    }

    _bus.add_device(new Device(_bus, vf_bdf & 0xFF, this));
  }

  return true;
}

PARAM(nubus,
      {
	new NubusManager(mb.bus_hwpcicfg, mb.clock());
      },
      "nubus - PCI bus manager");

// EOF
