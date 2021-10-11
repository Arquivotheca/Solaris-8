#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile	1.7	99/09/30 SMI

TMPDIR   = c:\tmp

PROTO=..\..\proto

install: all $(PROTO)
	copy mboot $(PROTO)\mboot

all: mboot

mboot: mboot.exe exe2bin.exe
	copy exe2bin.exe $(TMPDIR)\exe2bin.exe
	$(TMPDIR)\exe2bin -strip_to_entry mboot.exe mboot
	del $(TMPDIR)\exe2bin.exe

mboot.exe: mboot.s
	ml /DFARDATA=1 /c /Cp /Cx /Fl /Sa /Zf /Zp1 /Sg /Tamboot.s
	link /M /NOD /nog /NOE /PACKC mboot, mboot.exe, mboot.map,,;

exe2bin.exe: ..\..\exe2bin.c
	@cl /AS /nologo ..\..\exe2bin.c

clean:
	-del mboot
	-del *.map
	-del *.exe
	-del *.obj
	-del *.lst

clobber: clean
	-del $(PROTO)\mboot

$(PROTO):
	-md $@
