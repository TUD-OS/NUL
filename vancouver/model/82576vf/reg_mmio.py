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
      'important' : 100,
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
      'rc' :     11<<4,
      'mutable' : ~3,        # These bits are handled in VMMB_cb
      'initial' : 0x80,         # RSTD (PF has completed reset)
      'callback' : 'VMMB_cb',
      },
    { 'name' : 'rVTIVAR',      'offset' : 0x1700, 'initial' : 0, 'mutable' : 0x83838383 },
    { 'name' : 'rVTIVAR_MISC', 'offset' : 0x1740, 'initial' : 0, 'mutable' : 0x83 },
    ]

# Interrupt moderation
for n in range(3):
    rset.append({'name' : 'rVTEITR%d' % n,
                 'offset' : 0x1680 + 4*n,
                 'initial' : 0,
                 'callback' : 'VTEITR_cb'})

# Mailbox memory
for n in range(0x10):
    rset.append({'name' : 'rVFMBX%d' % n,
                 'offset' : 0x800 + 4*n,
                 'initial' : 0})

# EOF
