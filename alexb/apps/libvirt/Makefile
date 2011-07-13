all: libvirt-0.9.2.tar.gz
	tar -xzf libvirt-0.9.2.tar.gz
	( cd libvirt-0.9.2 && patch -p1 <../nova.diff && autoreconf && \
	 ./autogen.sh --without-uml --without-openvz --without-vmware --without-lxc --without-esx --without-remote --with-nova && \
	 $(MAKE) )

libvirt-0.9.2.tar.gz:
	wget ftp://libvirt.org/libvirt/$@

clean:
	rm libvirt-0.9.2/ -rf
