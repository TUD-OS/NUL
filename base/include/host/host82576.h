// -*- Mode: C++ -*-
/** @file
 * Host Intel 82576 driver.
 *
 * Copyright (C) 2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <nul/types.h>

class Base82576VF {
protected:
  enum VRegister {
    VTCTRL      = 0x0000/4,
    VTSTATUS    = 0x0008/4,
    VTFRTIMER   = 0x1048/4,

    VTEICS      = 0x1520/4,
    VTEIMS      = 0x1524/4,
    VTEIMC      = 0x1528/4,
    VTEIAC      = 0x152C/4,
    VTEIAM      = 0x1530/4,
    VTEICR      = 0x1580/4,
    VTEITR0     = 0x1680/4,
    VTEITR1     = 0x1684/4,
    VTEITR2     = 0x1688/4,
    VTIVAR      = 0x1700/4,
    VTIVAR_MISC = 0x1740/4,

    RDBAL0      = 0x2800/4,
    RDBAH0      = 0x2804/4,
    RDLEN0      = 0x2808/4,
    SRRCTL0     = 0x280C/4,
    RDH0        = 0x2810/4,
    RDT0        = 0x2818/4,
    RXDCTL0     = 0x2828/4,

    TDBAL0      = 0x3800/4,
    TDBAH0      = 0x3804/4,
    TDLEN0      = 0x3808/4,
    TDH0        = 0x3810/4,
    TXCTL0      = 0x3814/4,
    TDT0        = 0x3818/4,
    TXDCTL0     = 0x3828/4,
    TDWBAL0     = 0x3838/4,
    TDWBAH0     = 0x383C/4,

    VMB         = 0x0C40/4, 	// 8.14.3
    VBMEM       = 0x0800/4,	// 8.14.4
  };
};

class Base82576 {
protected:

  enum Spec {
    RX_QUEUES = 16,
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
    IAM       = 0x01510/4,	// Interrupt auto Mask
    GPIE      = 0x01514/4,	// General Purpose Interrupt Enable

    EICS      = 0x01520/4,	// Extended Interrupt Cause Set
    EIMS      = 0x01524/4,	// Extended Interrupt Mask Set
    EIMC      = 0x01528/4,	// Extended Interrupt Mask Clear
    EIAC      = 0x0152C/4,	// Extended Interrupt Auto Mask Clear
    EIAM      = 0x01530/4,	// Extended Interrupt Auto Mask Enable
    EICR      = 0x01580/4,
    IVAR0     = 0x01700/4,	// Interrupt Vector Allocation (0..7)
    IVAR_MISC = 0x01740/4,

    // General
    RCTL      = 0x00100/4,	// RX Control
    TCTL      = 0x00400/4,
    RXPBS     = 0x02404/4,	// RX Packet Buffer Size

    // RX
    DTXCTL    = 0x03590/4,	// DMA Tx Control

    RAL0      = 0x05400/4,
    RAH0      = 0x05404/4,

    // VT
    VT_CTL    = 0x0581C/4,	// VMDq control register
    VMOLR0    = 0x05AD0/4,	// VM Offload Register 0 (0..7)
    MRQC      = 0x05818/4,	// Multiple Receive Queues Command
    PFMB0     = 0x00C00/4,	// PF Mailbox (+ 4*VFno)
    PFMBMEM   = 0x00800/4,	// PF Mailbox Memory (+ 0x40*VFno)

    MBVFICR   = 0x00C80/4,	// Mailbox VF Interrupt Causes (R/W1C) (loword -> send msg?, hiword -> acked msg?)
    MBVFIMR   = 0x00C84/4,	// Mailbox VF Interrupt Mask
    VFLRE     = 0x00C88/4,	// VF Function Level Reset (R/W1C)
    VFRE      = 0x00C8C/4,	// VF Receive enable  (lower 8 bits)
    VFTE      = 0x00C90/4,	// VF Transmit enable (lower 8 bits)
    QDE       = 0x02408/4,	// Queue drop enable (lower 16 bits)
    DTXSWC    = 0x03500/4,	// DMA Tx Switch Control
    WVBR      = 0x03554/4, 	// Wrong VM Behaviour (RC)

    // Misc
    TCPTIMER  = 0x0104C/4,	// TCP Timer
    MPC       = 0x04010/4,	// Missed Packets Count
    GPRC      = 0x04074/4,	// Good Packets Receive Count
    GPTC      = 0x04080/4,	// Good Packets Transmitted Count
    RNBC      = 0x040A0/4,	// Receive No Buffer Count
    
    // Filtering
    UTA0      = 0x0A000/4,
    MTA0      = 0x05200/4,
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

  enum DTXCTL {
    DTX_MDP_EN        = 1U << 5, // Malicious Driver Protection Enable
    DTX_SPOOF_INT     = 1U << 6, // Interrupt on Spoof Behaviour Detection
  };

  enum VT {
    VT_CTL_DIS_DEF_POOL = 1U<<29, // Disable default pool
    VT_CTL_REP_ENABLE   = 1U<<30, // Replication enable

    VMOLR_RPML_MASK     = (1U << 14) - 1,
    VMOLR_LPE           = 1U << 16, // Long Packets Enable
    VMOLR_AUPE          = 1U << 24, // Accept Untagged Packets Enable
    VMOLR_ROMPE         = 1U << 25, // Accept packets that match the MTA
    VMOLR_ROPE          = 1U << 26, // Accept packets that match the UTA
    VMOLR_BAM           = 1U << 27, // Broadcast Accept
    VMOLR_MPE           = 1U << 28, // Multicast Promiscuous Enable
    VMOLR_STRVLAN       = 1U << 30, // Strip VLAN Tag
    VMOLR_DEFAULT       = 1U << 31, // Default value. Must always be set.
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
    IRQ_TXDW      = 1U<<7,	// Transmit Descriptor Writeback
    IRQ_LSC       = 1U<<2,	// Link Status Change
    IRQ_RXO       = 1U<<6,	// Receiver Overrun
    IRQ_RXDW      = 1U<<7,	// Receive Descriptor Writeback
    IRQ_VMMB      = 1U<<8,	// VM Mailbox/FLR
    IRQ_FER       = 1U<<22,	// Fatal Error
    IRQ_NFER      = 1U<<23,	// Non-Fatal Error
    IRQ_SWD       = 1U<<26,	// Software Watchdog expired
    IRQ_TIMER     = 1U<<30,
    IRQ_INTA      = 1U<<31,     // INTA asserted (not available for MSIs)

    EIRQ_TIMER    = 1U<<30,
    EIRQ_OTHER    = 1U<<31,

    GPIE_NSICR         = 1U<<0,	// Non-Selective Interrupt Clear
    GPIE_MULTIPLE_MSIX = 1U<<4,
    GPIE_EIAME         = 1U<<30,
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

    RAH_AV        = 1U<<31,
    RAH_POOLSEL_SHIFT = 18,

    MRQC_MRQE_011 = 3U,		// Filter via MAC, always use default queue of pool

    SRRCTL_DESCTYPE_ADV1B = 1U<<25, // Advanced descriptor mode, 1 buffer (no header split)
    SRRCTL_DROP_EN  = 1U<<31,	    // Drop Enable
    
  };

  enum TransmitControl {
    TCTL_TXEN     = 1U<<1,	// Transmit Enable
    TCTL_PSP      = 1U<<3,	// Pad Short Packets

    DTXSWC_LOOP_EN = 1U<<31,	// VMDq Loopback enable (VM-to-VM traffic)
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

  // Hardware interface
  enum PFMBX {
    Sts = 1U << 0,		// Status/Message ready (WO)
    Ack = 1U << 1,		// VF Message received (WO)
    VFU = 1U << 2,		// Buffer taken by VF (RO for PF?)
    PFU = 1U << 3,		// Buffer taken by PF (WO)
    RVFU = 1U << 4,		// Reset VFU (Clears VFU) (WO)
  };

  // Software interface
  enum MBX {
    VF_RESET         = 0x0001U,
    VF_SET_MAC_ADDR  = 0x0002U,
    VF_SET_MULTICAST = 0x0003U,
    VF_SET_LPE       = 0x0005U,

    VF_SET_PROMISC   = 0x0006U,
    VF_SET_PROMISC_MULTICAST = 0x2U << 16,
    VF_SET_PROMISC_UNICAST   = 0x4U << 16,

    PF_CONTROL_MSG   = 0x0100U,

    CMD_ACK          = 0x80000000U,
    CMD_NACK         = 0x40000000U,
    CTS              = 0x20000000U,
  };

};

// EOF
