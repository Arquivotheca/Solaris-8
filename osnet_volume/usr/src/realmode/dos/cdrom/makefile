#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile	1.3	99/09/30 SMI

# Makefile for CDROM bootstraps and El Torito headers.

TMPDIR	= c:\tmp
AFLAGS=/nologo /c
LFLAGS=/nologo
DESTDIR=..\proto

.SUFFIXES: .s .cd

.s.cd:
	ml /c $(AFLAGS) $*.s
	link $(LFLAGS) $*.obj;
	copy ..\exe2bin.exe $(TMPDIR)\exe2bin.exe
	$(TMPDIR)\exe2bin -strip_to_entry $*.exe $*.cd
	del $(TMPDIR)\exe2bin.exe

install: all $(DESTDIR)\brvd.cd $(DESTDIR)\catalog.cd $(DESTDIR)\mboot.cd \
	$(DESTDIR)\pboot.cd

$(DESTDIR)\brvd.cd: brvd.cd
        copy brvd.cd $(DESTDIR)\brvd.cd

$(DESTDIR)\catalog.cd: catalog.cd
        copy catalog.cd $(DESTDIR)\catalog.cd

$(DESTDIR)\mboot.cd: mboot.cd
        copy mboot.cd $(DESTDIR)\mboot.cd

$(DESTDIR)\pboot.cd: pboot.cd
        copy pboot.cd $(DESTDIR)\pboot.cd

all: brvd.cd catalog.cd mboot.cd pboot.cd

clean:
	if exist *.obj del *.obj
	if exist *.exe del *.exe
	if exist *.cd del *.cd
