all: .build.stamp

libvirt-0.9.6.tar.gz:
	wget ftp://libvirt.org/libvirt/$@

.patch.stamp: nova.diff libvirt-0.9.6.tar.gz
	rm -rf libvirt-0.9.6
	tar  -xzf libvirt-0.9.6.tar.gz
	cd libvirt-0.9.6 && patch -p1 <../nova.diff 
	touch $@

.configure.stamp: .patch.stamp
	cd libvirt-0.9.6 && \
	 ./autogen.sh --without-uml --without-openvz --without-vmware --without-lxc --without-esx --without-remote --with-nova --without-macvtap
	touch $@

.build.stamp: .configure.stamp
	 $(MAKE) -C libvirt-0.9.6
	touch $@

$(DESTDIR)/.install.stamp: .build.stamp
	$(MAKE) -C libvirt-0.9.6/ install
	touch $@

install: $(DESTDIR)/.install.stamp

clean:
	rm libvirt-0.9.6/ -rf
