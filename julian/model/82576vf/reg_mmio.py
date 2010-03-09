# -*- Mode: Python -*-

name = "MMIO"

rset = [
    # Normal R/W register
    { 'name' : 'rVTCTRL',
      'offset'  : 0x0,
      'initial' : 0},
    # Normal R/O register
    { 'name' : 'rSTATUS',
      'offset' : 0x8,
      'initial' : 0,
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
      'rc': True },
    { 'name' : 'rVTEICS',
      'offset' : 0x1520,
      'set' : 'rVTEICR',
      'callback' : 'VTEICR_cb',
      'w1s' : True,             # Write 1s to set. 0s are ignored
      'write-only' : True },
    ]

# EOF
