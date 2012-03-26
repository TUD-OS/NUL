.PHONY: vmware-image
vmware-image:
	./nova-vmware --dump > nova-vmware.tmp
	../novaboot -c /dev/null --iso=nova-vmware.iso -- nova-vmware.tmp ../gdisk
	ovftool -tt=vmx --powerOffTarget --overwrite  vmware/NOVA.vmx .
	echo 'vhv.enable = "TRUE"' >> NOVA/NOVA.vmx
