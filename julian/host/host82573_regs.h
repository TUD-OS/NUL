// -*- Mode: C++ */

#pragma once

enum Registers {
  CTRL      = 0x0000U/4,
  STATUS    = 0x0008U/4,
  EERD      = 0x0014U/4,
  CTRL_EXT  = 0x0018U/4,
  PBA       = 0x1000U/4,
  PBS       = 0x1008U/4,

  ICR       = 0x00C0U/4,
  ITR       = 0x00C4U/4,
  ICS       = 0x00C8U/4,
  IMS       = 0x00D0U/4,
  IMC       = 0x00D8U/4,
  EIAC      = 0x00DCU/4,
  IAM       = 0x00E0U/4,
  IVAR      = 0x00E4U/4,

  RXDCTL    = 0x2828U/4,
  RADV      = 0x282CU/4,

  RXCSUM    = 0x5000U/4,
  RAL0      = 0x5400U/4,
  RAH0      = 0x5404U/4,

  RCTL      = 0x0100U/4,
  RCTL1     = 0x0104U/4,
  PSRCTL    = 0x2170U/4,
  RDBAL     = 0x2800U/4,
  RDBAH     = 0x2804U/4,
  RDLEN     = 0x2808U/4,
  RDH       = 0x2810U/4,
  RDT       = 0x2818U/4,
  RDTR      = 0x2820U/4,
  MRQC      = 0x5818U/4,
  RFCTL     = 0x5008U/4,

  TCTL      = 0x0400U/4,
  TDH       = 0x3810U/4,
  TDT       = 0x3818U/4,
  TDBAL     = 0x3800U/4,
  TDBAH     = 0x3804U/4,
  TDLEN     = 0x3808U/4,
  TIDV      = 0x3820U/4,
  TXDCTL    = 0x3828U/4,
  
  MTA       = 0x5200U/4,
};

enum {
  CTRL_SWRST          = 1U<<26,
  CTRL_MASTER_DISABLE = 1U<<2,
  CTRL_PHY_RST        = 1U<<31,
  CTRL_SLU            = 1U<<6,  // Not in 82578

  ICR_TXDW   = 1U<<0,
  ICR_LSC    = 1U<<2,
  ICR_RXT    = 1U<<7,
  ICR_RXO    = 1U<<6,
  ICR_INTA   = 1U<<31,
  
  STATUS_LU  = 1U<<1,
  STATUS_MASTER_ENABLE_STATUS = 1U<<19,
  
};

// EOF
