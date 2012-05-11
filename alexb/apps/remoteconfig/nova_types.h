enum LIBVIRT_NOVA_OPCODE {
  NOVA_OP_FAILED   = 0x8,
  NOVA_OP_SUCCEEDED = 0x9,
  NOVA_OP_FAILED_OUT_OF_MEMORY = 0xa,
  NOVA_PACKET_LEN  = 0x40,
  NOVA_NUM_OF_ACTIVE_DOMAINS = 0x20,
  NOVA_NUM_OF_DEFINED_DOMAINS,
  NOVA_LIST_ACTIVE_DOMAINS,
  NOVA_LIST_DEFINED_DOMAINS,
  NOVA_GET_NAME_ID,
  NOVA_GET_NAME_UUID,
  NOVA_GET_NAME,
  NOVA_GET_VM_INFO,
  NOVA_VM_START,
  NOVA_VM_PAUSE,
  NOVA_VM_RESUME,
  NOVA_VM_DESTROY,
  NOVA_UNSUPPORTED_VERSION,
  NOVA_ENABLE_EVENT,
  NOVA_DISABLE_EVENT,
  NOVA_EVENT,
  NOVA_HW_INFO,
  NOVA_AUTH,
  NOVA_ATOMIC_RULE,
  NOVA_GET_DISK_INFO,
  NOVA_GET_NET_INFO,
  EVENT_REBOOT = 0xbbbb,
  EVENT_UNSERVED_IOACCESS = 0xbbc0,
  EVENT_DMAR_ACCESS = 0xbbd0,
  EVENT_VDEV_HONEYPOT = 0xbbd1,
};
// Elisp function to update op2string() server.cc: (update-nul-libvirt-server-cc)

struct incoming_packet {
  uint16_t version;
  uint16_t opcode;
  unsigned char opspecific;
} __attribute__((packed));

struct outgoing_packet {
  uint16_t version;
  uint16_t opcode;
  uint8_t  result;
  unsigned char opspecific;
} __attribute__((packed));
