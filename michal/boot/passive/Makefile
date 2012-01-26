all: nova-82573 nova-82576 nova-vmware nova-qemu

nova-82573: nova
	sed -e 's/@SIGMA0_ARGS@/host82573:0 hostahci/' < $< > $@
	chmod +x $@

nova-82576: nova
	sed -e 's/@SIGMA0_ARGS@/host82576:0 host82576vf:0,0,1 hostahci/' < $< > $@
	chmod +x $@

nova-vmware: nova
	sed -e 's/@SIGMA0_ARGS@/host82573:0 hostide/' < $< > $@
	chmod +x $@

nova-qemu: nova
	sed -e 's/@SIGMA0_ARGS@/hostne2k hostide/' < $< > $@
	chmod +x $@

vmware-image:
	../novaboot --iso=passive-vmware.iso -- passive-82573 ../gdisk
	ovftool --powerOffTarget --overwrite  ~/vmware/Passive/Passive.vmx /tmp/passive-nova-vm
	ovftool vmware/passive.vmx
