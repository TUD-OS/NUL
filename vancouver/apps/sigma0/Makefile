IMAGE=bin/sigma0.nova
# sigma0
OBJ          += sigma0.o asm.o
# environment
OBJ	     += vprintf.o logging.o simplemalloc.o
# host driver
OBJ	     += hostio.o hostpcicfg.o hostacpi.o hostkeyboard.o hostsink.o hostserial.o hostpit.o hostvga.o hostrtc.o hosthpet.o
# disk driver
OBJ	     += hostide.o hostahci.o virtualdisk.o
# vesa support
OBJ	     += hostvesa.o memorycontroller.o novahalifax.o pit8254.o sysctrlport.o pcihostbridge.o pcidirect.o directio.o
# others
OBJ          += aliases.o simplebootp.o
INCLUDES=-I../ -I../../include/ -I../../include/libc
vpath %.S  :../sys
vpath %.cc :../../host/ ../../models ../../executor ../sys
include ../Makeconf