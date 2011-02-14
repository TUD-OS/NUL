#!/bin/sh

BINARIES=$HOME/boot

if [ -z "$1" ]; then
    TMPDIR=`mktemp -d`
else
    TMPDIR=$1
fi

echo "Working in $TMPDIR ..."; sleep 1

ISO_DIR=$TMPDIR/iso
rm -fdR $ISO_DIR

mkdir -p $ISO_DIR/nul
mkdir -p $ISO_DIR/tools

cp -v $BINARIES/nul/{hypervisor,sigma0.nul.gz,vancouver.nul.gz} $ISO_DIR/nul/
cp -v $BINARIES/tools/{unzip,bender.gz,munich} $ISO_DIR/tools/

# Syslinux

SYSLINUX_DIR=/usr/share/syslinux

mkdir -p $ISO_DIR/isolinux
cp $SYSLINUX_DIR/isolinux.bin   $ISO_DIR/isolinux
#cp $SYSLINUX_DIR/com32/modules/*.c32 $ISO_DIR/isolinux
cp $SYSLINUX_DIR/menu.c32 $ISO_DIR/isolinux
cp $SYSLINUX_DIR/mboot.c32 $ISO_DIR/isolinux

# Scenarios

mkdir -p $ISO_DIR/demo/linux || exit 4

cp $BINARIES/linux/bzImage.x $ISO_DIR/demo/linux/bzImage.x || exit 4
cp $BINARIES/linux/initrd-udo $ISO_DIR/demo/linux/initrd || exit 4

# ISOLINUX config

SARGS="S0_DEFAULT hostvga:0x40000,0x40000 hostkeyb:0,0x60,1,12 tracebuffer:10240 "
SARGS_K="S0_DEFAULT hostvga:0x40000,0x40000 hostkeyb:0,0x60,1,12,1,1 tracebuffer:10240 "

cat > $ISO_DIR/isolinux/isolinux.cfg <<EOF

DEFAULT menu

MENU HELPMSGROW 21
MENU HELPMSGENDROW -2
MENU TITLE NOVA snapshot 2011.2

 LABEL novanbridge
 MENU LABEL NOVA: Network Bridge
 KERNEL mboot.c32
 APPEND /tools/unzip --- \
        /tools/bender.gz --- \
        /nul/hypervisor spinner dmar --- \
        /nul/sigma0_nul.gz $SARGS script_start:1 script_wait:100 script_start:1,1,3 --- \
        /nul/vancouver_nul.gz --- \
        /tools/munich --- \
        /nul/hypervisor --- \
        /demo/linux/bzImage.x  --- \
        /demo/linux/initrd --- \
        /demo/linux/bridge.nulconfig
TEXT HELP
Starts NOVA and four Linux VMs. The first VM gets the first network card
assigned via PCI passthrough and bridges traffic from the virtual network.
Use Win-[arrow] to navigate between hypervisor consoles.
ENDTEXT


 LABEL novanbridge2
 MENU LABEL NOVA: Network Bridge (keyb)
 KERNEL mboot.c32
 APPEND /tools/unzip --- \
        /tools/bender.gz --- \
        /nul/hypervisor spinner dmar --- \
        /nul/sigma0_nul.gz $SARGS_K script_start:1 script_wait:100 script_start:1,1,3 --- \
        /nul/vancouver_nul.gz --- \
        /tools/munich --- \
        /nul/hypervisor --- \
        /demo/linux/bzImage.x  --- \
        /demo/linux/initrd --- \
        /demo/linux/bridge.nulconfig
TEXT HELP
Starts NOVA and four Linux VMs. The first VM gets the first network card
assigned via PCI passthrough and bridges traffic from the virtual network.
Use Win-[arrow] to navigate between hypervisor consoles.
ENDTEXT

 LABEL novavesa
 MENU LABEL NOVA: VESA
 KERNEL mboot.c32
 APPEND /tools/unzip --- \
        /tools/bender.gz --- \
        /nul/hypervisor spinner dmar --- \
        /nul/sigma0_nul.gz $SARGS_K hostvesa script_start:1 --- \
        /nul/vancouver_nul.gz --- \
        /tools/munich --- \
        /nul/hypervisor --- \
        /demo/linux/bzImage.x  --- \
        /demo/linux/initrd --- \
        /demo/linux/bridge.nulconfig
TEXT HELP
Starts NOVA and four Linux VMs. The first VM gets the first network card
assigned via PCI passthrough and bridges traffic from the virtual network.
Use Win-[arrow] to navigate between hypervisor consoles.
ENDTEXT


EOF

cat > $ISO_DIR/demo/linux/bridge.nulconfig <<EOF
name::/s0/log name::/s0/timer name::/s0/fs/rom sigma0::mem:128 sigma0::dma sigma0::log ||
rom:///nul/vancouver_nul.gz vga_fbsize=4096 PC_PS2 dpci:2,,0 82576vf ||
rom:///tools/munich ||
rom:///demo/linux/bzImage.x root=/dev/ram0 quiet ||
rom:///demo/linux/initrd

EOF

mkisofs -o $TMPDIR/nul.iso -iso-level 2 \
        -b isolinux/isolinux.bin -c isolinux/boot.cat \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        $ISO_DIR || exit 4

isohybrid $TMPDIR/nul.iso

# EOF
