enum LIBVIRT_NOVA_OPCODE {
  NOVA_OP_FAILED   = 0x8,
  NOVA_OP_SUCCEEDED = 0x9,
  NOVA_PACKET_LEN  = 0x40,
  NOVA_NUM_OF_ACTIVE_DOMAINS = 0x20,
  NOVA_NUM_OF_DEFINED_DOMAINS,
  NOVA_LIST_ACTIVE_DOMAINS,
  NOVA_LIST_DEFINED_DOMAINS,
  NOVA_GET_NAME_ID,
  NOVA_GET_NAME_UUID,
  NOVA_GET_NAME,
  NOVA_VM_START,
  NOVA_VM_DESTROY,
  NOVA_UNSUPPORTED_VERSION,
  NOVA_ENABLE_EVENT,
  NOVA_DISABLE_EVENT,
  NOVA_EVENT,
  EVENT_REBOOT = 0xbbbb
};

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
