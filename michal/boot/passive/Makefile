.PHONY: vmware-image
vmware-image:
	./nova-vmware --dump > /tmp/nova-vmware
	../novaboot -c /dev/null --iso=nova-vmware.iso -- /tmp/nova-vmware ../gdisk
	ovftool -tt=vmx --powerOffTarget --overwrite  vmware/NOVA.vmx .
	echo 'vhv.enable = "TRUE"' >> NOVA/NOVA.vmx
