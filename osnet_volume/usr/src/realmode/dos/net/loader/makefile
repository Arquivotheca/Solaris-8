#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile	1.7	99/09/30 SMI

TMPDIR   = c:\tmp

install: all loader.bin

loader.bin: loader.exe
	copy ..\..\exe2bin.exe $(TMPDIR)\exe2bin.exe
	$(TMPDIR)\exe2bin loader.exe loader.bin
	del $(TMPDIR)\exe2bin.exe

all: loader.exe

loader.exe: loader.s
	ml /c /nologo loader.s
	link /nologo loader;

clean:
	-del *.obj
	-del *.lst
	-del *.bin
	-del *.exe
