#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile	1.7	99/09/30 SMI

TMPDIR   = c:\tmp

install: all
	copy pboot ..\..\proto\pboot

all: pboot

pboot: pboot.exe
	copy ..\..\exe2bin.exe $(TMPDIR)\exe2bin.exe
	$(TMPDIR)\exe2bin -strip_to_entry pboot.exe pboot
	del $(TMPDIR)\exe2bin.exe

pboot.exe: pboot.s
	ml /DFARDATA= /c /Cp /Cx /Fl /Sa /Zf /Zd /Zi /Sg /Tapboot.s
	link /nologo /M /NOD /NOE /PACKC pboot, pboot.exe, pboot.map,,;
clean:
	-del pboot
	-del *.map
	-del *.exe
	-del *.obj
	-del *.lst
