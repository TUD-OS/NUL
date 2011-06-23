all:
	wget ftp://libvirt.org/libvirt/libvirt-0.9.2.tar.gz
	tar -xzf libvirt-0.9.2.tar.gz
	
	cd libvirt-0.9.2 ; \
	if [ "$$?" -eq "0" ]; then \
		patch -p1 <../nova.diff; \
		if [ "$$?" -eq "0" ]; then autoconf; \
			if [ "$$?" -eq "0" ]; then \
				./autogen.sh --without-uml --without-openvz --without-vmware --without-lxc --without-esx --without-remote --with-nova ; \
				if [ "$$?" -eq "0" ]; then make; fi; \
			fi; \
		fi; \
	fi

clean:
	rm libvirt-0.9.2* -rf
