# -*- Mode: Python -*-

name = "PCI"

rset = [
    { 'name' : 'rPCIID',
      'offset' : 0,
      'initial' : 0x10ca8086,
      'constant'  : True },     # Constant implies read-only
    { 'name' : 'rPCISTSCTRL',
      'offset' : 4,
      'initial' : 0x100000,
      'mutable' : 0x6 },  # Bus Master Enable, Memory Decode
    { 'name' : 'rPCICCRVID', 'offset' :    8, 'initial' : 0x02000001, 'constant' : True },
    { 'name' : 'rBIST',      'offset' : 0x0C, 'initial' : 0x0, 'constant' : True },
    { 'name' : 'rPCIBAR0',   'offset' : 0x10, 'initial' : 0x0, 'mutable' : ~0x3FFF },
    { 'name' : 'rPCIBAR3',   'offset' : 0x1C, 'initial' : 0x0, 'mutable' : ~0x0FFF },
    { 'name' : 'rPCISUBSYS', 'offset' : 0x2C, 'initial' : 0x8086, 'constant' : True },
    { 'name' : 'rPCICAPPTR', 'offset' : 0x34, 'initial' : 0x70, 'constant' : True },

    # MSI-X Cap
    { 'name' : 'rPCIMSIX0',  'offset' : 0x70, 'initial' : 0x20011, 'mutable' : 3<<14 },
    { 'name' : 'rPCIMSIXTBA', 'offset' : 0x74, 'initial' : 0x3, 'constant' : True },
    { 'name' : 'rPCIMSIXPBA', 'offset' : 0x78, 'initial' : 0x183, 'constant' : True },

    # PCIe Cap
    # { 'name' : 'rPCIXCAP0', 'offset' : 0xA0, 'initial' : 0x20010, 'constant' : True },
    # { 'name' : 'rPCIXCAP1', 'offset' : 0xA4, 'initial' : 0x10000d82, 'constant' : True },
    # { 'name' : 'rPCIXCAP2', 'offset' : 0xA8, 'initial' : 0, 'mutable' : 1<<15,
    #   'callback' : 'PCI_check_flr'},
    # ...

    ]

# EOF
