#pragma once
#include "host/hostpci.h"

/**
 * A helper for PCI config space access for virtual functions.
 */
class HostVfPci : public HostPci
{
public:
  enum {
    BAR_SIZE                = 4,
    SRIOV_VF_BAR0           = 0x24U,
  };

  /**
   * Return the base and size of a VF BAR (inside a SR-IOV capability).
   */
  unsigned long long vf_bar_base_size(unsigned bdf, unsigned vf_no, unsigned no, unsigned long long &size, bool *is64bit=0) {

    unsigned sriov_cap = find_extended_cap(bdf, EXTCAP_SRIOV);
    if (!sriov_cap) return -1;
    size =  bar_size(bdf, sriov_cap + SRIOV_VF_BAR0 + no*4, is64bit);
    return  bar_base(bdf, sriov_cap + SRIOV_VF_BAR0 + no*4) + vf_no * size;
  }


  void read_all_vf_bars(unsigned bdf, unsigned vf_no, unsigned long long *base, unsigned long long *size) {

    memset(base, 0, MAX_BAR*sizeof(*base));
    memset(size, 0, MAX_BAR*sizeof(*size));

    // read bars
    for (unsigned i=0; i < count_bars(bdf); i++) {
      bool is64bit;
      base[i] = vf_bar_base_size(bdf, vf_no, i, size[i], &is64bit);
      if (is64bit) i++;
    }
  }


  /** Compute BDF of a particular VF. */
  unsigned vf_bdf(unsigned parent_bdf, unsigned vf_no)
  {
    unsigned sriov_cap = find_extended_cap(parent_bdf, EXTCAP_SRIOV);
    if (!sriov_cap) return 0;

    unsigned vf_offset = conf_read(parent_bdf, sriov_cap + 0x14);
    unsigned vf_stride = vf_offset >> 16;
    vf_offset &= 0xFFFF;
    return parent_bdf + vf_stride*vf_no + vf_offset;
  }

  unsigned vf_device_id(unsigned parent_bdf)
  {
    unsigned sriov_cap = find_extended_cap(parent_bdf, EXTCAP_SRIOV);
    if (!sriov_cap) return 0;

    return (conf_read(parent_bdf, sriov_cap + 0x18) & 0xFFFF0000)
      | (conf_read(parent_bdf, 0) & 0xFFFF);
  }
 HostVfPci(DBus<MessagePciConfig> &bus_pcicfg, DBus<MessageHostOp> &bus_hostop) : HostPci(bus_pcicfg, bus_hostop) {}
};
