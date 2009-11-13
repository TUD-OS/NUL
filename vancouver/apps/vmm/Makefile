IMAGE=bin/vancouver.nova
# VMM
OBJ	     +=	vancouver.o asm.o vprintf.o logging.o simplemalloc.o
# host driver
OBJ	     += hostio.o hostpcicfg.o hostkeyboard.o hostsink.o hostserial.o hostpit.o irqrouting.o
# generic io and mem
OBJ	     += aliases.o directio.o nullio.o memorycontroller.o
# keyboard, mouse, serial
OBJ          += keyboardcontroller.o ps2keyboard.o ps2mouse.o serial16550.o serial2kbd.o kbd2serial.o
# pit, pic, sysctrl, rtc
OBJ          += pic8259.o pit8254.o sysctrlport.o rtc146818.o
# pci
OBJ	     += pcihostbridge.o
# sata
OBJ	     += ahcicontroller.o satadrive.o
# vga
OBJ	     += vga.o
# basic executors
OBJ	     += cpuidexecutor.o msrexecutor.o triplefaultexecutor.o irqexecutor.o initexecutor.o novahalifax.o
# bios and multiboot
OBJ          += virtualbios.o vesabios.o multiboot.o
# contrib qemu
OBJ	     += qemu_net.o qemu.o ne2000.o e1000.o net-checksum.o

vpath	%.cc . ../../models/ ../../executor ../../host ../sys
vpath	%.c  ../../contrib/qemu/modified ../../contrib/qemu/src/ ../../contrib/qemu/src/hw/
vpath	%.S  . ../sys

INCLUDES=-I../../include -I../../include/libc -I../ -I../../executor/ -I../../contrib/qemu/modified/ -I../../contrib/qemu/src
include ../Makeconf

CCFLAGS+=-m32 -MD -Os \
	 -ffunction-sections -fstrict-aliasing --param max-inline-insns-single=100 -mregparm=3 -fomit-frame-pointer \
	 -minline-all-stringops  -g   -Wall \
	 -nostdinc  -I. $(INCLUDES)  -I$(shell $(CC) -m32 -print-file-name=include)

bin/%.o: %.c
	$(call MESSAGE,CMP,$@)
	$(CC) $(CCFLAGS) -c -o $@ $<


