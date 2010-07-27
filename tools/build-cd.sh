#!/bin/sh

NOVA_URL=os:/srv/git/repos/nova.git
NUL_URL=os:/srv/git/repos/nul.git
MORBO_URL=os:~jsteckli/git-private/morbo.git

co_git() {
    if [ ! -d $1/$3 ]; then
	( cd "$1" && git clone "$2" "$3" ) || exit 1
	( cd "$1/$3" && git checkout $4 ) || exit 1
    else
	( cd "$1/$3" && git pull ) || exit 1
    fi
}

tftp_get() {
    atftp --get -r "$2" -l "$3" "$1" || exit 4
}

if [ -z "$1" ]; then
    TMPDIR=`mktemp -d`
else
    TMPDIR=$1
fi

echo "Working in $TMPDIR ..."; sleep 1

co_git $TMPDIR $NOVA_URL  nova  master
co_git $TMPDIR $NUL_URL   nul   release-2010.7
co_git $TMPDIR $MORBO_URL morbo master

#co_git $TMPDIR $NOVA_URL  nova-snapshot  master
#co_git $TMPDIR $NUL_URL   nul-snapshot   release-2010.7


ISO_DIR=$TMPDIR/iso
rm -fdR $ISO_DIR
mkdir -p $ISO_DIR/nul
mkdir -p $ISO_DIR/tools

scons -C "$TMPDIR/morbo"      || exit 2
scons -C "$TMPDIR/nul/build"  || exit 2
make  -C "$TMPDIR/nova/build" || exit 2
cp -v $TMPDIR/nova/build/hypervisor $ISO_DIR/nul/
cp -v $TMPDIR/nul/build/bin/apps/*.nul $ISO_DIR/nul/
cp -v $TMPDIR/nul/build/bin/boot/* $ISO_DIR/tools/

# Syslinux

SYSLINUX_VER=4.02
SYSLINUX_URL=http://www.kernel.org/pub/linux/utils/boot/syslinux/syslinux-${SYSLINUX_VER}.tar.bz2
SYSLINUX_DIR=$TMPDIR/syslinux-${SYSLINUX_VER}

if [ ! -d $SYSLINUX_DIR ]; then
    ( cd $TMPDIR ; wget -O - "$SYSLINUX_URL" | tar xjf - ) || exit 3
fi

( cd $SYSLINUX_DIR ; make )

mkdir -p $ISO_DIR/isolinux
cp $SYSLINUX_DIR/core/isolinux.bin   $ISO_DIR/isolinux
cp $SYSLINUX_DIR/com32/modules/*.c32 $ISO_DIR/isolinux
cp $SYSLINUX_DIR/com32/menu/menu.c32 $ISO_DIR/isolinux
cp $SYSLINUX_DIR/com32/mboot/mboot.c32 $ISO_DIR/isolinux

# Scenarios

mkdir -p $ISO_DIR/demo/linux || exit 4

tftp_get nova /linux/bzImage-2.6.34-32 $ISO_DIR/demo/linux/bzImage

# Hardcode mode 0x317 (1024x768)
cp $ISO_DIR/demo/linux/bzImage  $ISO_DIR/demo/linux/bzImage.x
vidmode $ISO_DIR/demo/linux/bzImage.x 791
tftp_get nova /linux/slackware-32.bz2 $ISO_DIR/demo/linux/initrd

tftp_get nova /demo/nova.slides $ISO_DIR/demo/nova.slides

# Generate tarballs
(cd $ISO_DIR && git clone ../nul nul-snapshot )    || exit 5
(cd $ISO_DIR && git clone ../nova nova-snapshot ) || exit 5

rm -dRf $ISO_DIR/nul-snapshot/.git
rm -dRf $ISO_DIR/nova-snapshot/.git

bsdtar cvjf $ISO_DIR/nul.tar.bz2 -C $ISO_DIR  nul-snapshot       || exit 5 
bsdtar cvjf $ISO_DIR/nova.tar.bz2 -C $ISO_DIR nova-snapshot      || exit 5
rm -dRf $ISO_DIR/nul-snapshot $ISO_DIR/nova-snapshot

# ISOLINUX config

SARGS="S0_DEFAULT hostvga:0x40000,0x40000 hostkeyb:0,0x60,1,12 startlate:0xfffffffd"
VMMARGS="PC_PS2 kbmodifier:0x40000"

cat > $ISO_DIR/isolinux/isolinux.cfg <<EOF

DEFAULT menu

MENU HELPMSGROW 21
MENU HELPMSGENDROW -2
MENU TITLE NOVA snapshot

 LABEL novabasic
 MENU LABEL NOVA (basic)
 KERNEL mboot.c32
 APPEND /nul/hypervisor serial --- \
        /nul/sigma0.nul $SARGS --- \
        /nul/vancouver.nul $VMMARGS 82576vf:0xf7ce0000,0xf7cc0000 sigma0::dma sigma0::mem:48 --- \
        /tools/munich sigma0::attach --- \
        /demo/linux/bzImage root=/dev/ram0 clocksource=tsc sigma0::attach --- \
        /demo/linux/initrd sigma0::attach --- \
        /nul/tutor.nul 
TEXT HELP
This is a basic setup that should run on all boxes.
Use Win-[1-9] to start different VM configurations. Linux VMs share a
virtual network and can communicate with each other.
ENDTEXT

 LABEL novaiommu
 MENU LABEL NOVA (IOMMU)
 KERNEL mboot.c32
 APPEND /nul/hypervisor serial dmar --- \
        /nul/sigma0.nul $SARGS --- \
        /nul/vancouver.nul $VMMARGS dpci:2,0,0 82576vf:0xf7ce0000,0xf7cc0000 sigma0::dma sigma0::mem:48 --- \
        /tools/munich sigma0::attach --- \
        /demo/linux/bzImage root=/dev/ram0 clocksource=tsc sigma0::attach --- \
        /demo/linux/initrd sigma0::attach --- \
        /nul/tutor.nul
TEXT HELP
This setup has DMA remapping. The first Linux VM has direct access to
your first network card and creates a bridge to the virtual
network. You need Intel VT-d support for this.
ENDTEXT

 LABEL novax
 MENU LABEL NOVA (VESA+IOMMU)
 KERNEL mboot.c32
 APPEND /nul/hypervisor serial dmar --- \
        /nul/sigma0.nul $SARGS hostvesa --- \
        /nul/vancouver.nul vga_fbsize:4096 $VMMARGS dpci:2,0,0 82576vf:0xf7ce0000,0xf7cc0000 sigma0::dma sigma0::mem:48 --- \
        /tools/munich sigma0::attach --- \
        /demo/linux/bzImage.x root=/dev/ram0 clocksource=tsc sigma0::attach --- \
        /demo/linux/initrd sigma0::attach --- \
        /nul/rocknshine.nul sigma0::mem:8 --- \
        /demo/nova.slides sigma0::attach
TEXT HELP
This is the same setup as above, but with VESA enabled.
ENDTEXT
EOF

mkisofs -o $TMPDIR/nul.iso -iso-level 2 \
        -b isolinux/isolinux.bin -c isolinux/boot.cat \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        $ISO_DIR || exit 4

# EOF
