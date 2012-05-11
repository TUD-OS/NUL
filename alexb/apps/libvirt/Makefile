all: .build.stamp

VERSION=0.9.9
libvirt-$(VERSION).tar.gz:
	wget ftp://libvirt.org/libvirt/$@

.patch.stamp: nova.diff libvirt-$(VERSION).tar.gz
	rm -rf libvirt-$(VERSION)
	tar  -xzf libvirt-$(VERSION).tar.gz
	cd libvirt-$(VERSION) && patch -p1 <../nova.diff
	touch $@

libvirt-repo/.git:
	cd $(shell git rev-parse --show-toplevel) && git submodule update --init $(shell git rev-parse --show-prefix)libvirt-repo

LIBVIRT_HASH=$(shell git --git-dir=libvirt-repo/.git rev-parse HEAD)
LIBVIRT_HASH_OLD=$(shell cat .version.stamp)

# Update .version.stamp whenever the HEAD of the libvirt repostory is updated
ifneq ($(wildcard .version.stamp),)
ifneq ($(LIBVIRT_HASH),$(LIBVIRT_HASH_OLD))
$(info New libvirt version: rebuild forced)
.PHONY: .version.stamp
endif
endif

.version.stamp: libvirt-repo/.git
	@echo newer: $?
	echo $(LIBVIRT_HASH) > $@

.configure.stamp: .version.stamp
	cd libvirt-repo && autoreconf -v && ./autogen.sh		\
	--without-uml --without-openvz --without-vmware --without-lxc	\
	--without-esx --with-nova --without-macvtap --without-python	\
	--without-remote --without-libvirtd --without-qemu		\
	--without-vbox --without-avahi --without-selinux

	touch $@

.build.stamp: .configure.stamp
	 $(MAKE) -C libvirt-repo
	touch $@

$(DESTDIR)/.install.stamp: .build.stamp
	$(MAKE) -C libvirt-repo/ install
	touch $@

install: $(DESTDIR)/.install.stamp

clean:
	rm -rf libvirt-$(VERSION)-nova
	rm -f .build.stamp 
	rm -f .configure.stamp
	rm -f .patch.stamp

FORCE:

libvirt-$(VERSION)-nova: libvirt-repo FORCE
	rm -rf $@
	git --git-dir=$</.git checkout-index --prefix=$@/ -a -f
	cd $@ && autoreconf -v -f


dist: libvirt-$(VERSION)-nova
	tar cf - $< | gzip > $<.tar.gz

nova.diff: FORCE
	git --git-dir=libvirt-repo/.git diff upstream master > $@
