.PHONY: vmware-image
vmware-image:
	./nova-vmware --dump > /tmp/nova-vmware
	./nova-82573 --dump > /tmp/nova-82573
	./nova-82576 --dump > /tmp/nova-82576
	./nova-qemu --dump > /tmp/nova-qemu
	../novaboot -c /dev/null --iso=nova-vmware.iso -- /tmp/nova-vmware /tmp/nova-82576 /tmp/nova-82573 /tmp/nova-qemu ../gdisk
	ovftool -tt=vmx --powerOffTarget --overwrite  vmware/NOVA.vmx .
	echo 'vhv.enable = "TRUE"' >> NOVA/NOVA.vmx
