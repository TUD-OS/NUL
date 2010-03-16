# -*- Mode: Python -*-

name = "MMIO"

rset = [
    # Normal R/W register
    { 'name' : 'rVTCTRL', 'offset'  : 0x0, 'initial' : 0,
      'callback' : 'VTCTRL_cb' },
    # Normal R/O register
    { 'name' : 'rSTATUS', 'offset'  : 0x8, 'initial' : int('10000011',2), # 1GB/s, UP, FD
      'read-only' : True },
    # Free Running Timer
    { 'name' : 'rVTFRTIMER',
      'offset' : 0x1048,
      'read-only' : True,
      'read-compute' : 'VTFRTIMER_compute' },
    # RC/W1C
    { 'name' : 'rVTEICR',
      'offset' : 0x1580,
      'initial' : 0,
      'w1c' : True,
      'rc': 0xFFFFFFFF },
    { 'name' : 'rVTEICS',
      'offset' : 0x1520,
      'set' : 'rVTEICR',
      'callback' : 'VTEICS_cb',
      'w1s' : True,             # Write 1s to set. 0s are ignored
      'write-only' : True },
    # Interrupt Mask
    { 'name' : 'rVTEIMS',
      'offset' : 0x1524,
      'initial' : 0,
      'w1s' : True },
    { 'name' : 'rVTEIMC',
      'offset' : 0x1528,
      'write-only' : True,
      'w1c' : True,
      'set' : 'rVTEIMS' },
    # Auto-Clear
    { 'name' : 'rVTEIAC',
      'offset' : 0x152c,
      'initial' : 0 },
    # Auto-Mask
    { 'name' : 'rVTEIAM',
      'offset' : 0x1530,
      'initial' : 0 },
    { 'name' : 'rVMMB',
      'offset' : 0xC40,
      'rc' : 0b1011<<4,
      'mutable' : ~0b11,        # These bits are handled in VMMB_cb
      'initial' : 0x80,         # RSTD (PF has completed reset)
      'callback' : 'VMMB_cb',
      },
    { 'name' : 'rVTIVAR',      'offset' : 0x1700, 'initial' : 0, 'mutable' : 0x83838383 },
    { 'name' : 'rVTIVAR_MISC', 'offset' : 0x1740, 'initial' : 0, 'mutable' : 0x83 },
    ]

# Mailbox memory
for n in range(0x10):
    rset.append({'name' : 'rVFMBX%d' % n,
                 'offset' : 0x800 + 4*n,
                 'initial' : 0})

# Queues
for n in range(2):
    rset.append({'name' : 'rRDBAL%d' % n, 'offset' : 0x2800 + n*256,
                 'initial' : 0, 'mutable' : ~0x3F})
    rset.append({'name' : 'rRDBAH%d' % n, 'offset' : 0x2804 + n*256,
                 'initial' : 0, 'mutable' : ~0x3F})
    rset.append({'name' : 'rRDLEN%d' % n, 'offset' : 0x2808 + n*256,
                 'initial' : 0, 'mutable' : 0xFFFFC0})
    rset.append({'name' : 'rRDH%d' % n, 'offset' : 0x2810 + n*256,
                 'initial' : 0, 'mutable' : 0xFFFF})
    rset.append({'name' : 'rRDT%d' % n, 'offset' : 0x2818 + n*256,
                 'initial' : 0, 'mutable' : 0xFFFF})
    rset.append({'name' : 'rRXDCTL%d' % n, 'offset' : 0x2828 + n*256,
                 'initial' : 1<<16 | ((1<<26) if n == 0 else 0),
                 'mutable' : ~(1<<26) # SWFLUSH is WC
                 })
    rset.append({'name' : 'rSRRCTL%d' % n, 'offset' : 0x280C + n*256,
                 'initial' : 0x400 | (0x80000000 if n != 0 else 0),
                 'mutable' : 0xFFFF})

# EOF
