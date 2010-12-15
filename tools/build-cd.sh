#!/bin/sh

NUL_URL=os:/srv/git/repos/nul.git

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

co_git $TMPDIR $NUL_URL   nul   master

ISO_DIR=$TMPDIR/iso
rm -fdR $ISO_DIR

scons -C "$TMPDIR/nul/build"  target_cc=$HOME/local-install/bin/gcc target_cxx=$HOME/local-install/bin/g++ tftp=$ISO_DIR \
    $ISO_DIR/nul/hypervisor \
    $ISO_DIR/nul/sigma0.nul.gz \
    $ISO_DIR/nul/vancouver.nul.gz \
    $ISO_DIR/tools/santamonica \
    $ISO_DIR/tools/munich  \
    $ISO_DIR/tools/bender  \
 || exit 2

# Syslinux

SYSLINUX_VER=4.03
SYSLINUX_URL=http://www.kernel.org/pub/linux/utils/boot/syslinux/syslinux-${SYSLINUX_VER}.tar.bz2
SYSLINUX_DIR=$TMPDIR/syslinux-${SYSLINUX_VER}

if [ ! -d $SYSLINUX_DIR ]; then
    ( cd $TMPDIR ; wget -O - "$SYSLINUX_URL" | tar xjf - ) || exit 3
fi

( cd $SYSLINUX_DIR ; make )

mkdir -p $ISO_DIR/isolinux
cp $SYSLINUX_DIR/core/isolinux.bin   $ISO_DIR/isolinux
#cp $SYSLINUX_DIR/com32/modules/*.c32 $ISO_DIR/isolinux
cp $SYSLINUX_DIR/com32/menu/menu.c32 $ISO_DIR/isolinux
cp $SYSLINUX_DIR/com32/mboot/mboot.c32 $ISO_DIR/isolinux

# Scenarios

mkdir -p $ISO_DIR/demo/linux || exit 4

cp /srv/tftp/linux/bzImage-js $ISO_DIR/demo/linux/bzImage || exit 4
cp /srv/tftp/linux/initrd-js.lzma $ISO_DIR/demo/linux/initrd || exit 4

# ISOLINUX config

SARGS="S0_DEFAULT hostvga:0x40000,0x40000 hostkeyb:0,0x60,1,12 hostserial tracebuffer:32768 script script_start:1 quota::guid namespace::/s0 name::/s0/timer"
VMMARGS="PC_PS2 kbmodifier:0x40000 name::/s0/log name::/s0/timer"

cat > $ISO_DIR/isolinux/isolinux.cfg <<EOF

DEFAULT menu

MENU HELPMSGROW 21
MENU HELPMSGENDROW -2
MENU TITLE NOVA snapshot 2010.12

 LABEL novabasic
 MENU LABEL NOVA
 KERNEL mboot.c32
 APPEND /tools/bender --- \
        /tools/santamonica --- \
        /nul/hypervisor nospinner dmar serial --- \
        /nul/sigma0_nul.gz $SARGS --- \
        /nul/vancouver_nul.gz $VMMARGS dpci:2,0,0 sigma0::dma sigma0::mem:512 --- \
        /tools/munich sigma0::attach --- \
        /demo/linux/bzImage root=/dev/ram0 clocksource=tsc sigma0::attach --- \
        /demo/linux/initrd sigma0::attach 
TEXT HELP
Starts NOVA and one Linux VM that gets the first network card directly
assigned. Use Win-[arrow] to navigate between hypervisor consoles.
ENDTEXT
EOF

mkisofs -o $TMPDIR/nul.iso -iso-level 2 \
        -b isolinux/isolinux.bin -c isolinux/boot.cat \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        $ISO_DIR || exit 4

# EOF
