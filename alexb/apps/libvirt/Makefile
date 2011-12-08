all: .build.stamp

VERSION=0.9.6
libvirt-$(VERSION).tar.gz:
	wget ftp://libvirt.org/libvirt/$@

.patch.stamp: nova.diff libvirt-$(VERSION).tar.gz
	rm -rf libvirt-$(VERSION)
	tar  -xzf libvirt-$(VERSION).tar.gz
	cd libvirt-$(VERSION) && patch -p1 <../nova.diff
	touch $@

.configure.stamp: .patch.stamp
	cd libvirt-$(VERSION) && \
	 autoreconf && \
	 ./autogen.sh --without-uml --without-openvz --without-vmware --without-lxc --without-esx --with-nova --without-macvtap
	touch $@

.build.stamp: .configure.stamp
	 $(MAKE) -C libvirt-$(VERSION)
	touch $@

$(DESTDIR)/.install.stamp: .build.stamp
	$(MAKE) -C libvirt-$(VERSION)/ install
	touch $@

install: $(DESTDIR)/.install.stamp

clean:
	rm -rf libvirt-$(VERSION)
	rm -f .build.stamp 
	rm -f .configure.stamp
	rm -f .patch.stamp
