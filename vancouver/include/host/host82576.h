// -*- Mode: C++ -*-

#pragma once

#include <driver/logging.h>
#include <vmm/motherboard.h>
#include <cstdint>

class Base82576 {
protected:

  enum Spec {
    RX_QUEUES = 16,
  };

  enum PCI {
    MSIX_ENABLE = (1UL << 31),
  };
  
  enum Register {
    // Spec p.399

    // General
    CTRL      = 0x0000/4,
    STATUS    = 0x0008/4,
    CTRL_EXT  = 0x0018/4,
    MDIC      = 0x0020/4,
    SERDESCTL = 0x0024/4,

    FRTIMER   = 0x1048/4,	// Free Running Timer

    // Interrupts
    ICR       = 0x01500/4,
    ICS       = 0x01504/4,
    IMS       = 0x01508/4,	// Interrupt Mask Set/Read
    IMC       = 0x0150C/4,	// Interrupt Mask Clear
    GPIE      = 0x01514/4,	// General Purpose Interrupt Enable

    EIMS      = 0x01528/4,	// Extended Interrupt Mask Clear
    EICR      = 0x01580/4,

    // General
    RCTL      = 0x00100/4,	// RX Control
    TCTL      = 0x00400/4,
    RXPBS     = 0x02404/4,	// RX Packet Buffer Size

    RAL0      = 0x05400/4,
    RAH0      = 0x05404/4,

    // VT
    VT_CTL    = 0x0581C/4,	// VMDq control register
    VFRE      = 0x00C8C/4,	// VF Receive enable  (lower 8 bits)
    VFTE      = 0x00C90/4,	// VF Transmit enable (lower 8 bits)
    QDE       = 0x02408/4,	// Queue drop enable (lower 16 bits)
    DTXSWC    = 0x03500/4,	// DMA Tx Switch Control

    // Misc
    TCPTIMER  = 0x0104C/4,	// TCP Timer
    GPRC      = 0x04074/4,	// Good Packets Receive Count
    GPTC      = 0x04080/4,	// Good Packets Transmitted Count
  };

  enum TCPTimer {
    TCPTIMER_KICKSTART = 1U << 8,
    TCPTIMER_ENABLE    = 1U << 9,
    TCPTIMER_FINISH    = 1U << 10,
    TCPTIMER_LOOP      = 1U << 11,
  };

  enum DTXSWC {
    DTX_VMDQ_LOOPBACK = 1U << 31,
  };

  enum VT {
    VT_CTL_DIS_DEF_POOL = 1U<<29, // Disable default pool
    VT_CTL_REP_ENABLE   = 1U<<30, // Replication enable
  };

  enum Ctrl {
    CTRL_GIO_MASTER_DISABLE = 1U<<2,
    CTRL_SLU      = 1U<<6,	// Set Link Up
    CTRL_FRCSPD   = 1U<<11,	// Force Speed
    CTRL_FRCDPLX  = 1U<<12,	// Force Duplex
    CTRL_RST      = 1U<<26,	// Device Reset
    CTRL_VME      = 1U<<30,	// VLAN Mode Enable
  };

  enum CtrlExt {
    CTRL_EXT_PFRSTD    = 0x1U<<14, // PF Reset Done
    CTRL_EXT_LINK_MODE = 0x3U<<22, // Set to 0 for Copper link.
  };

  enum Status {
    STATUS_FD     = 0x1U, // Full duplex (0 = Half Duplex, 1 = Full Duplex)
    STATUS_LU     = 0x2U, // Link Up
    STATUS_LAN_ID = 0x3U<<2,	// LAN ID (2-bit)
    STATUS_SPEED  = 0x3U<<6,
    STATUS_SPEED_SHIFT = 6U,

    STATUS_NUMVF  = 0xFU<<14,	// Number of VFs
    STATUS_NUMVF_SHIFT = 14U,
    STATUS_IOV    = 1<<18U,	// Value of VF Enable Bit
    STATUS_GIO_MASTER_ENABLE = 1<<19U,
  };

  enum StatusSpeed {
    SPEED_10M     = 0x0,
    SPEED_100M    = 0x1,
    SPEED_1000M   = 0x2,
    SPEED_1000M2  = 0x3,
  };

  enum Interrupt {
    IRQ_TXDW      = 1<<7,	// Transmit Descriptor Writeback
    IRQ_LSC       = 1<<2,       // Link Status Change
    IRQ_RXO       = 1<<6,	// Receiver Overrun
    IRQ_RXDW      = 1<<7,	// Receive Descriptor Writeback
    IRQ_VMMB      = 1<<8,	// VM Mailbox/FLR
    IRQ_FER       = 1<<22,      // Fatal Error
    IRQ_NFER      = 1<<23,      // Non-Fatal Error
    IRQ_SWD       = 1<<26,      // Software Watchdog expired
    IRQ_INTA      = 1U<<31,     // INTA asserted (not available for MSIs)

    GPIE_MULTIPLE_MSIX = 1U<<4,
    GPIE_PBA           = 1U<<31,
  };

  enum ReceiveControl {
    RCTL_RXEN     = 1<<1,	// Receive Enable
    RCTL_SBP      = 1<<2,	// Store Bad Packets
    RCTL_UPE      = 1<<3,	// Unicast Promiscuous
    RCTL_MPE      = 1<<4,	// Multicast Promiscuous
    RCTL_LPE      = 1<<5,	// Long Packet Enable
    RCTL_BAM      = 1<<15,	// Broadcast Accept Mode
    RCTL_BSIZE    = 3<<16,	// Buffer Size (default: 0 = 2K)
  };

  enum TransmitControl {
    TCTL_TXEN     = 1U<<1,	// Transmit Enable
    TCTL_PSP      = 1U<<3,	// Pad Short Packets
  };

  enum PacketType {
    TYPE_L2       = 1<<11,	// L2 packet
    TYPE_IPV4     = 1,
    TYPE_IPV4E    = 1<<1,
    TYPE_IPV6     = 1<<2,
    TYPE_IPV6E    = 1<<3,
    TYPE_TCP      = 1<<4,
    TYPE_UDP      = 1<<5,

  };

  // Misc
  Clock *_clock;

  void spin(unsigned micros)
  {
    timevalue done = _clock->abstime(micros, 1000000);
    while (_clock->time() < done)
      Cpu::pause();
  }

  bool wait(volatile uint32_t &reg, uint32_t mask, uint32_t value,
	    unsigned timeout_micros = 1000000 /* 1s */)
  {
    timevalue timeout = _clock->abstime(timeout_micros, 1000000);

    while ((reg & mask) != value) {
      Cpu::pause();
      if (_clock->time() >= timeout)
	return false;
    }
    return true;
  }

  /// Logging
  unsigned _msg_level;
  uint16_t _bdf;

  // Messages are tagged with one or more constants from this
  // bitfield. You can disable certain kinds of messages in the
  // constructor.
  enum MessageLevel {
    INFO  = 1<<0,
    DEBUG = 1<<1,
    PCI   = 1<<2,
    IRQ   = 1<<3,
    RX    = 1<<4,
    TX    = 1<<5,

    ALL   = ~0U,
  };

  __attribute__ ((format (printf, 3, 4)))
  void msg(unsigned level, const char *msg, ...)
  {
    if ((level & _msg_level) != 0) {
      va_list ap;
      va_start(ap, msg);
      Logging::printf("82576 %02x: ", _bdf & 0xFF);
      Logging::vprintf(msg, ap);
      va_end(ap);
    }
  }

  Base82576(Clock *clock, unsigned msg_level, uint16_t bdf)
    : _clock(clock), _msg_level(msg_level), _bdf(bdf)
  {}
};

// EOF
