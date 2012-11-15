#!/usr/bin/env novaboot
# -*-sh-*-
QEMU_FLAGS=-cpu phenom -net nic,model=ne2k_pci -net user,hostfwd=udp:127.0.0.1:5555-:5555,hostfwd=tcp:127.0.0.1:7777-:7777
bin/apps/sigma0.nul tracebuffer_verbose verbose S0_DEFAULT hostserial hostvga \
	hostkeyb:0,0x60,1,12,1 hostne2k script_start:1 script_waitchild
bin/apps/test_ip.nul
ip.nulconfig <<EOF
  sigma0::mem:3 name::/s0/admission name::/s0/log name::/s0/timer ||
  rom://bin/apps/test_ip.nul
EOF
