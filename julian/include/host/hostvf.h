
/**
 * A helper for PCI config space access for virtual functions.
 */
class HostVfPci : public HostPci
{
public:
  enum {
    BAR_SIZE                = 4,
    SRIOV_VF_BAR0           = 0x24U,
    CAP_PCI_EXPRESS         = 0x10U,
    EXTCAP_ARI              = 0x000EU,
    EXTCAP_SRIOV            = 0x0010U,

  };

  unsigned find_extended_cap(unsigned bdf, unsigned short id)
  {
    unsigned long header, offset;

    if ((find_cap(bdf, CAP_PCI_EXPRESS)) && (~0UL != conf_read(bdf, 0x100)))
      for (offset = 0x100, header = conf_read(bdf, offset);
	   offset != 0;
	   offset = header>>20, header = conf_read(bdf, offset))
	if ((header & 0xFFFF) == id)
	  return offset;

    return 0;
  }

  /** Return the base of a VF BAR (inside a SR-IOV capability).
   */
  unsigned long long vf_bar_base(unsigned bdf, unsigned no)
  {
    unsigned sriov_cap = find_extended_cap(bdf, EXTCAP_SRIOV);
    if (!sriov_cap) return -1;
    return bar_base(bdf, sriov_cap + SRIOV_VF_BAR0 + no*4);
  }

  /** Return the size of a VF BAR (inside a SR-IOV capability
   */
  unsigned long long vf_bar_size(unsigned bdf, unsigned no, bool *is64bit = 0)
  {
    unsigned sriov_cap = find_extended_cap(bdf, EXTCAP_SRIOV);
    if (!sriov_cap) return -1;
    return bar_size(bdf, sriov_cap + SRIOV_VF_BAR0 + no*4, is64bit);
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
};
