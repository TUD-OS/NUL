#!/bin/sh

BINARIES=$HOME/boot

if [ -z "$1" ]; then
    TMPDIR=`mktemp -d`
else
    TMPDIR=$1
fi

echo "Working in $TMPDIR ..."; sleep 1

ISO_DIR=$TMPDIR/iso
rm -fr $ISO_DIR

mkdir -p $ISO_DIR/nul
mkdir -p $ISO_DIR/tools

cp -v $BINARIES/nul/hypervisor $ISO_DIR/nul/
cp -v $BINARIES/nul/{sigma0,vancouver,rocknshine,tutor,cycleburner}.nul.gz $ISO_DIR/nul/
cp -v $BINARIES/tools/{unzip,bender.gz,munich} $ISO_DIR/tools/
cp -rv $BINARIES/pistachio $ISO_DIR/
cp -rv $BINARIES/fiasco    $ISO_DIR/

# Syslinux

SYSLINUX_DIR=/usr/share/syslinux

mkdir -p $ISO_DIR/isolinux
cp $SYSLINUX_DIR/isolinux.bin   $ISO_DIR/isolinux
#cp $SYSLINUX_DIR/com32/modules/*.c32 $ISO_DIR/isolinux
cp $SYSLINUX_DIR/menu.c32 $ISO_DIR/isolinux
cp $SYSLINUX_DIR/mboot.c32 $ISO_DIR/isolinux

# Scenarios

mkdir -p $ISO_DIR/linux || exit 4

cat $BINARIES/tutor.nulconfig | sed 's,rom://,rom:///,' | sed 's/\.nul\.gz/_nul.gz/' > $ISO_DIR/tutor.nulconfig || exit 4
cat $BINARIES/rocknshine.nulconfig | sed 's,rom://nul/r,rom:///nul/r,' | sed 's,rom://nul/e,rom:///nul/e,' | sed 's/\.nul\.gz/_nul.gz/' > $ISO_DIR/rocknshine.nulconfig || exit 4
cat $BINARIES/cycleburner.nulconfig | sed 's,rom://,rom:///,' | sed 's/\.nul\.gz/_nul.gz/' > $ISO_DIR/cycleburner.nulconfig || exit 4
cat $BINARIES/grml.nulconfig | sed 's,rom://,rom:///,' | sed 's/\.nul\.gz/_nul.gz/' > $ISO_DIR/grml.nulconfig || exit 4
cat $BINARIES/linux.nulconfig | sed 's,rom://,rom:///,' | sed 's/\.nul\.gz/_nul.gz/' | sed 's/initrd-udo/initrd/' > $ISO_DIR/linux.nulconfig || exit 4
cat $ISO_DIR/linux.nulconfig | sed 's/bzImage\.x/bzImage/' > $ISO_DIR/linux_novesa.nulconfig
cat $BINARIES/pistachio/pistachio.nulconfig | sed 's,rom://,rom:///,' | sed 's/\.nul\.gz/_nul.gz/' | sed 's/i686-kernel/i686_kernel/' > $ISO_DIR/pistachio/pistachio.nulconfig
cat $BINARIES/fiasco/fiasco.nulconfig | sed 's,rom://,rom:///,' | sed 's/\.nul\.gz/_nul.gz/' > $ISO_DIR/fiasco/fiasco.nulconfig


cp $BINARIES/linux/grml.iso $ISO_DIR/linux/ || exit 4
cp $BINARIES/nul/eurosys.slides $ISO_DIR/nul/ || exit 4
cp $BINARIES/linux/bzImage.x $ISO_DIR/linux/bzImage.x || exit 4
cp $BINARIES/linux/bzImage $ISO_DIR/linux/bzImage || exit 4
cp $BINARIES/linux/initrd-udo $ISO_DIR/linux/initrd || exit 4

# ISOLINUX config

SARGS="S0_DEFAULT hostvga:0x40000,0x40000 hostkeyb:0,0x60,1,12 tracebuffer:10240 vdisk:rom:///linux/grml.iso"
SARGS_K="S0_DEFAULT hostvga:0x40000,0x40000 hostkeyb:0,0x60,1,12,1,1 tracebuffer:10240 vdisk:rom:///linux/grml.iso "

BINARIES=" /nul/tutor_nul.gz --- /nul/cycleburner_nul.gz  --- /nul/rocknshine_nul.gz --- /nul/eurosys.slides  --- /nul/vancouver_nul.gz  --- /tools/munich --- /linux/bzImage.x --- /linux/bzImage --- /linux/initrd  --- /linux/grml.iso  --- /pistachio/i686_kernel  --- /pistachio/kickstart  --- /pistachio/sigma0  --- /pistachio/pingpong  --- /fiasco/fiasco  --- /fiasco/bootstrap  --- /fiasco/pingpong  --- /fiasco/roottask  --- /fiasco/sigma0  --- /tutor.nulconfig --- /linux.nulconfig  --- /grml.nulconfig  --- /pistachio/pistachio.nulconfig  --- /fiasco/fiasco.nulconfig  --- /cycleburner.nulconfig --- /rocknshine.nulconfig "

BINARIES_NOVESA=" /nul/tutor_nul.gz --- /nul/cycleburner_nul.gz  --- /nul/rocknshine_nul.gz --- /nul/eurosys.slides  --- /nul/vancouver_nul.gz  --- /tools/munich --- /linux/bzImage.x --- /linux/bzImage --- /linux/initrd  --- /linux/grml.iso  --- /pistachio/i686_kernel  --- /pistachio/kickstart  --- /pistachio/sigma0  --- /pistachio/pingpong  --- /fiasco/fiasco  --- /fiasco/bootstrap  --- /fiasco/pingpong  --- /fiasco/roottask  --- /fiasco/sigma0  --- /tutor.nulconfig --- /linux_novesa.nulconfig  --- /grml.nulconfig  --- /pistachio/pistachio.nulconfig  --- /fiasco/fiasco.nulconfig  --- /cycleburner.nulconfig --- /rocknshine.nulconfig "

cat > $ISO_DIR/isolinux/isolinux.cfg <<EOF

DEFAULT menu

MENU HELPMSGROW 21
MENU HELPMSGENDROW -2
MENU TITLE NOVA Userland 2011.2

 LABEL nova1
 MENU LABEL NOVA Userland Demo (VESA)
 KERNEL mboot.c32
 APPEND /tools/unzip --- \
        /tools/bender.gz --- \
        /nul/hypervisor spinner iommu --- \
        /nul/sigma0_nul.gz $SARGS hostvesa script_start:1 --- \
        $BINARIES
TEXT HELP
Needs VT-x/VT-d for full functionality.
ENDTEXT

 LABEL nova2
 MENU LABEL NOVA Userland Demo (No VESA)
 KERNEL mboot.c32
 APPEND /tools/unzip --- \
        /tools/bender.gz --- \
        /nul/hypervisor spinner iommu --- \
        /nul/sigma0_nul.gz $SARGS script_start:1 --- \
        $BINARIES_NOVESA
TEXT HELP
Use this if graphics are garbled.
ENDTEXT

 LABEL nova3
 MENU LABEL NOVA Userland Demo (VESA, broken keyboard)
 KERNEL mboot.c32
 APPEND /tools/unzip --- \
        /tools/bender.gz --- \
        /nul/hypervisor spinner iommu --- \
        /nul/sigma0_nul.gz $SARGS_K hostvesa script_start:1 --- \
        $BINARIES
TEXT HELP
Use this if your BIOS has weird USB legacy support.
ENDTEXT


 LABEL nova4
 MENU LABEL NOVA Userland Demo (No VESA, broken keyboard)
 KERNEL mboot.c32
 APPEND /tools/unzip --- \
        /tools/bender.gz --- \
        /nul/hypervisor spinner iommu --- \
        /nul/sigma0_nul.gz $SARGS_K script_start:1 --- \
        $BINARIES_NOVESA
TEXT HELP
You poor soul. :-)
ENDTEXT

EOF

mkisofs -o $TMPDIR/nul.iso -iso-level 2 \
        -b isolinux/isolinux.bin -c isolinux/boot.cat \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        $ISO_DIR || exit 4

isohybrid $TMPDIR/nul.iso

# EOF
