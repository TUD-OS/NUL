// -*- Mode: C++ -*-

#include <cstdint>
#include <nul/motherboard.h>
#include <model/pci.h>

class Model82576vf : public StaticReceiver<Model82576vf>
{

#include <model/82576vfmmio.inc>
#include <model/82576vfpci.inc>

  uint32_t VTFRTIMER_compute()
  {
    // XXX
    return 0;
  }

  void VTEICR_cb()
  {
    // XXX
  }

  void PCI_BAR0_cb()
  {
    // XXX
  }

  void PCI_BAR3_cb()
  {
    // XXX
  }

public:

  bool receive(MessagePciConfig &msg)
  {
    if (msg.bdf) return false;

    switch (msg.type) {
    case MessagePciConfig::TYPE_READ:
      msg.value = PCI_read(msg.dword<<2);
      break;
    case MessagePciConfig::TYPE_WRITE:
      PCI_write(msg.dword<<2, msg.value);
      break;
    }
    return true;
  }

  bool receive(MessageMemRead &msg)
  {
    // XXX Magic number, use & to avoid double compare
    if ((msg.phys < rPCIBAR0) || (msg.phys >= rPCIBAR0 + 0x4000))
      return false;
    // XXX Assert msg.count == 4
    *(reinterpret_cast<uint32_t *>(msg.ptr)) = MMIO_read(msg.phys - rPCIBAR0);
    return true;
  }

  bool receive(MessageMemWrite &msg)
  {
    // XXX Magic number, use & to avoid double compare
    if ((msg.phys < rPCIBAR0) || (msg.phys >= rPCIBAR0 + 0x4000))
      return false;
    // XXX Assert msg.count == 4
    MMIO_write(msg.phys - rPCIBAR0, *(reinterpret_cast<uint32_t *>(msg.ptr)));
    return true;
  }

  Model82576vf() 
  {
    PCI_init();
    MMIO_init();
  }

};

PARAM(model82576vf,
      {
	Model82576vf *dev = new Model82576vf;
	mb.bus_memwrite.add(dev, &Model82576vf::receive_static<MessageMemWrite>);
	mb.bus_memread.add(dev, &Model82576vf::receive_static<MessageMemRead>);
	mb.bus_pcicfg.add(dev, &Model82576vf::receive_static<MessagePciConfig>,
			  PciHelper::find_free_bdf(mb.bus_pcicfg, 
						   argv[1] // XXX
						   ));

      },
      "TODO"
      );
      

// EOF
