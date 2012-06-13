all: .patch.stamp .build.stamp event-test

ifneq ($(shell git config --get remote.origin.url),git://github.com/TUD-OS/NUL.git)
VERSION=repo
else
VERSION=0.9.9
endif

libvirt-$(VERSION).tar.gz:
	wget ftp://libvirt.org/libvirt/$@

ifneq ($(VERSION),repo)
.patch.stamp: nova.diff libvirt-$(VERSION).tar.gz
	rm -rf libvirt-$(VERSION)
	tar  -xzf libvirt-$(VERSION).tar.gz
	cd libvirt-$(VERSION) && patch -p1 <../nova.diff
	touch $@
else
.patch.stamp:
endif

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

ifeq ($(VERSION),repo)
ifeq ($(wildcard libvirt-repo/.git),)
.version.stamp: libvirt-repo/.git
endif

.version.stamp:
	echo $(LIBVIRT_HASH) > $@
else
.version.stamp:
endif

.configure.stamp: .version.stamp
	cd libvirt-$(VERSION) && autoreconf -v && ./autogen.sh		\
	--without-uml --without-openvz --without-vmware --without-lxc	\
	--without-esx --with-nova --without-macvtap --without-python	\
	--without-remote --without-libvirtd --without-qemu		\
	--without-vbox --without-avahi --without-selinux

	touch $@

.build.stamp: .configure.stamp
	 $(MAKE) -C libvirt-$(VERSION)
	touch $@

$(DESTDIR)/.install.stamp: .build.stamp
	$(MAKE) -C libvirt-$(VERSION)/ install
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
	git --git-dir=$</.git checkout-index --prefix=$(PWD)/$@/ -a -f
	cd $@ && autoreconf -v -f


dist: libvirt-$(VERSION)-nova
	tar cf - $< | gzip > $<.tar.gz

nova.diff:
	git --git-dir=libvirt-repo/.git diff origin/upstream master > $@

event-test: CFLAGS = -Ilibvirt-repo -Ilibvirt-repo/include -Wall -g
event-test: LDFLAGS = -Llibvirt-repo/src/.libs -lvirt
event-test: event-test.c | .build.stamp
