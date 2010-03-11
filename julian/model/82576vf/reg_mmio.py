# -*- Mode: Python -*-

name = "MMIO"

rset = [
    # Normal R/W register
    { 'name' : 'rVTCTRL', 'offset'  : 0x0, 'initial' : 0,
      'callback' : 'VTCTRL_cb' },
    # Normal R/O register
    { 'name' : 'rSTATUS', 'offset'  : 0x8, 'initial' : 0b10000011, # 1GB/s, UP, FD
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
      'callback' : 'VTEICR_cb',
      'w1s' : True,             # Write 1s to set. 0s are ignored
      'write-only' : True },
    { 'name' : 'rVMMB',
      'offset' : 0xC40,
      'rc' : 0b1011<<4,
      'mutable' : ~0b11,
      'initial' : 0,            # RSTI is on in the real hardware, but
                                # we don't care.
      'callback' : 'VMMB_cb',
      },
    { 'name' : 'rVTIVAR',      'offset' : 0x1700, 'initial' : 0, 'mutable' : 0x83838383 },
    { 'name' : 'rVTIVAR_MISC', 'offset' : 0x1740, 'initial' : 0, 'mutable' : 0x83 },
    ]

for n in range(10):
    rset.append({'name' : 'rVFMBX%d' % n,
                 'offset' : 0x800 + 4*n,
                 'initial' : 0})
                 

# EOF
