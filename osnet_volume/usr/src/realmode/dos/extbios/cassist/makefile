# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident "@(#)makefile	1.28	99/09/30 SMI\n"

#  Realmode pseudo-drivers:
#
#    The drivers ("*.bef" files) built in this directory are used by the
#    "bootconf" program to probe for standard ISA devices and to assign bus
#    resources to any such devices that may be installed.
#
#        com.bef   ...   Probes for serial ports
#        fdc.bef   ...   Probes for floppy disk controller
#        joyst.bef ...   Probes for game-port (joy stick)
#        key.bef   ...   Determines keyboard type
#        logi.bef  ...   Probes for Logitech bus mice
#        lpt.bef   ...   Probes for parallel ports
#        msm.bef   ...   Probes for Microsoft bus mice
#        nop.bef   ...   Dummy do-nothing driver
#	 pcic.bef  ...   Probes for an ISA pcmcia controller
#        ps2ms.bef ...   Probes for PS/2 mice
#        sbpro.bef ...   Probes for Sound Blaster cards
#        mwss.bef  ...   Probes for MWSS cards
#        boca.bef  ...   Probes for Boca 8 port serial card
#
#  Gross "cd" hack to get around DOS's lack of true process recursion (see
#  the $(CRT) production).  $(CM_DIR) is directory containing the driver
#  libraries, $(MY_DIR) is the path back from this directory to here.

CC_DIR = c:\c700
MY_DIR = ..\..\.
CM_DIR = .\common\lib
INC_DIRS = -I.\common\include -I..\..\common\include -I$(CC_DIR)\include

#    Here's what we're trying to build:

CRT = $(CM_DIR)\crt0.obj
#
#
BEF = com.bef fdc.bef joyst.bef key.bef logi.bef lpt.bef msm.bef\
	nop.bef pcic.bef ps2ms.bef sbpro.bef mwss.bef boca.bef

#    We need to refer to the libraries by two different names, one for
#    dependency checking, one for linking (what a crock!)

DLIB = $(CM_DIR)\common.lib $(CM_DIR)\slibce.lib $(CM_DIR)\oldnames.lib
LIBS = $(CM_DIR)\common + $(CM_DIR)\slibce + $(CM_DIR)\oldnames

#    Installation directories:
#
#    $(PROTO) is the root of the prototype solaris boot partition.  
#    $(DRIVERS) is the root solaris device drivers directory

PROTO = ..\..\proto

#    I prefer UNIX-like names!

CC = cl 
AS = masm
LD = link

#    The standard stuff:

#CFLAGS = /Od /Gs /G2 /nologo $(INC_DIRS)
CFLAGS = /O /Gs /G2 /nologo $(INC_DIRS)
#CFLAGS = /Fc /O /Gs /G2 /nologo $(INC_DIRS)
LFLAGS = /NOLOGO /M /PACKC /NOD /NOE

.SUFFIXES: .c .obj .bef

.c.obj:
	$(CC) /c $(CFLAGS) $*.c

.obj.bef:
	$(LD) $(LFLAGS) @<<link.dat
		$(CRT) $*.obj,
		$*.bef,
		$*.map,
		$(LIBS)

<<

all: $(CRT) $(BEF) 

#
#  Install production:  Build the pseudo-drivers and install them in the
#                       prototype directory under the appropriate names.
install: all
	copy com.bef $(PROTO)\com.bef
	copy fdc.bef $(PROTO)\fdc.bef
	copy joyst.bef $(PROTO)\joyst.bef
	copy key.bef $(PROTO)\key.bef
	copy logi.bef $(PROTO)\logi.bef
	copy lpt.bef $(PROTO)\lpt.bef
	copy msm.bef $(PROTO)\msm.bef
	copy nop.bef $(PROTO)\nop.bef
	copy pcic.bef $(PROTO)\pcic.bef
	copy ps2ms.bef $(PROTO)\ps2ms.bef
	copy sbpro.bef $(PROTO)\sbpro.bef
	copy mwss.bef $(PROTO)\mwss.bef
	copy boca.bef $(PROTO)\boca.bef

#  Support production:  Insures that the driver startup module and
#                       support libraries get built.
$(CRT)::
	cd $(CM_DIR)
	$(MAKE) /NOLOGO
	cd $(MY_DIR)

#  Drivers are dependent on the libraries and the startup module!
$(BEF): $(CRT) $(DLIB)

#  Throw out the leftovers
clean:
	cd common\lib
	$(MAKE) clean
	cd ..\..
	del *.bef
	del *.obj
	del *.map
	del *.cod


#                          ---  Dependencies ---

com.obj:	com.c
fdc.obj:	fdc.c
joyst.obj:	joyst.c
key.obj:	key.c
logi.obj:	logi.c
lpt.obj:	lpt.c
msm.obj:	msm.c
nop.obj:	nop.c
pcic.obj:	pcic.c pcic.h
ps2ms.obj:	ps2ms.c
sbpro.obj:	sbpro.h sbpro.c
mwss.obj:	mwss.h mwss.c

com.bef:	com.obj
fdc.bef:	fdc.obj
joyst.bef:	joyst.obj
key.bef:	key.obj
logi.bef:	logi.obj
lpt.bef:	lpt.obj
msm.bef:	msm.obj
nop.bef:	nop.obj
pcic.bef:	pcic.obj
ps2ms.bef:	ps2ms.obj
sbpro.bef:	sbpro.obj
mwss.bef:	mwss.obj
boca.bef:	boca.obj
