#include "qemu-common.h"
#include "net.h"
#include "hw/pci.h"


/* CPU */
void cpu_register_physical_memory(target_phys_addr_t start_addr, ram_addr_t size, ram_addr_t phys_offset) {}
void cpu_physical_memory_read(target_phys_addr_t addr,  uint8_t *buf, int len) {}
void cpu_physical_memory_write(target_phys_addr_t addr, const uint8_t *buf, int len) {}
int  cpu_register_io_memory(int io_index,  CPUReadMemoryFunc **mem_read, CPUWriteMemoryFunc **mem_write, void *opaque) {}
void qemu_register_coalesced_mmio(target_phys_addr_t addr, ram_addr_t size) {}
void cpu_unregister_io_memory(int table_address) {}

/* HW */
void qemu_register_reset(QEMUResetHandler *func, void *opaque) {}

/* ISA */
void isa_unassign_ioport(int start, int length) {}


/* NET */
void qemu_format_nic_info_str(VLANClientState *vc, uint8_t macaddr[6]) {}
void qemu_check_nic_model(NICInfo *nd, const char *model) {}
void qdev_get_macaddr(DeviceState *dev, uint8_t *macaddr) {}
VLANClientState *qdev_get_vlan_client(DeviceState *dev,
                                      NetCanReceive *can_receive,
                                      NetReceive *receive,
                                      NetReceiveIOV *receive_iov,
                                      NetCleanup *cleanup,
                                      void *opaque) {}


/* PCI */
PCIDevice *pci_register_device(PCIBus *bus, const char *name,
                               int instance_size, int devfn,
                               PCIConfigReadFunc *config_read,
                               PCIConfigWriteFunc *config_write) {}
void pci_register_io_region(PCIDevice *pci_dev, int region_num,
                            uint32_t size, int type,
                            PCIMapIORegionFunc *map_func) {}
void pci_register_bar(PCIDevice *pci_dev, int region_num,
		      uint32_t size, int type,
		      PCIMapIORegionFunc *map_func) {}
void pci_qdev_register(PCIDeviceInfo *info) {}


/* SAVE/LOAD */
void qemu_put_be16(QEMUFile *f, unsigned int v) {}
void qemu_put_be32(QEMUFile *f, unsigned int v) {}
void qemu_put_be64(QEMUFile *f, uint64_t v) {}
void qemu_put_byte(QEMUFile *f, int v) {}
void qemu_put_buffer(QEMUFile *f, const uint8_t *buf, int size) {}
unsigned int qemu_get_be16(QEMUFile *f) {}
unsigned int qemu_get_be32(QEMUFile *f) {}
uint64_t qemu_get_be64(QEMUFile *f)     {}
int qemu_get_buffer(QEMUFile *f, uint8_t *buf, int size) {}
int qemu_get_byte(QEMUFile *f) {}
void pci_device_save(PCIDevice *s, QEMUFile *f) {}
int pci_device_load(PCIDevice *s, QEMUFile *f) {}
int register_savevm(const char *idstr,
                    int instance_id,
                    int version_id,
                    SaveStateHandler *save_state,
                    LoadStateHandler *load_state,
                    void *opaque) {}
void unregister_savevm(const char *idstr, void *opaque) {}






void network_register_client(void **clientptr, void *opaque);
void network_receive_packet(void *opaque, const uint8_t *buf, int size)
{
  VLANClientState *vc = opaque;
  vc->receive(vc, buf, size);  
};

/* VLAN */
void qemu_send_packet(VLANClientState *vc1, const uint8_t *buf, int size) 
{
  network_send_packet((void **)(vc1+1), buf, size);
}


VLANClientState *qemu_new_vlan_client(VLANState *vlan,
                                      const char *model,
                                      const char *name,
                                      NetCanReceive *can_receive,
                                      NetReceive *receive,
                                      NetReceiveIOV *receive_iov,
                                      NetCleanup *cleanup,
                                      void *opaque) 
{
  VLANClientState * res = qemu_mallocz(sizeof(VLANClientState)+sizeof(void *));
  res->vlan = vlan;
  res->can_receive = can_receive;
  res->receive = receive;
  res->receive_iov = receive_iov;
  res->cleanup = cleanup;
  res->opaque = opaque;
  res->next = 0;
  network_register_client((void **)(res+1), res);
  return res;
}


void new_ne2000(unsigned base, void *irq, unsigned long long mac)
{

  struct NICInfo nd;
  nd.macaddr[0] = mac >> 0x28;
  nd.macaddr[1] = mac >> 0x20;
  nd.macaddr[2] = mac >> 0x18;
  nd.macaddr[3] = mac >> 0x10;
  nd.macaddr[4] = mac >> 0x08;
  nd.macaddr[5] = mac >> 0x00;
  nd.model = "ne2000";
  isa_ne2000_init(base, irq, &nd);
}

