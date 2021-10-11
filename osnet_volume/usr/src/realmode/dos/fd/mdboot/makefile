#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile	1.12	99/09/30 SMI

# Build Solaris MDB diskette loader

TMPDIR   = c:\tmp
#DEBUG=/Zf /Zd /Zi
DEBUG=
#LINKDEBUG=/co /inf
LINKDEBUG=

install: all

all: ..\..\proto\mdboot ..\..\proto\mdbootbp

..\..\proto\mdboot: mdboot.exe
	copy ..\..\exe2bin.exe $(TMPDIR)\exe2bin.exe 
	$(TMPDIR)\exe2bin -strip_to_entry mdboot.exe mdboot
	copy mdboot ..\..\proto\mdboot
	del $(TMPDIR)\exe2bin.exe

..\..\proto\mdbootbp: mdbootbp.exe
	copy ..\..\exe2bin.exe $(TMPDIR)\exe2bin.exe 
	$(TMPDIR)\exe2bin -strip_to_entry mdbootbp.exe mdbootbp
	copy mdbootbp ..\..\proto\mdbootbp
	del $(TMPDIR)\exe2bin.exe

callmdb.obj: callmdb.s
	masm callmdb.s

# special version for boot partition (temporary, we hope)
callmdbb.obj: callmdbb.s
	ml /c /Cp /Cx /Fl /Sa /Tacallmdbb.s

math.obj: math.s
	ml /c /Cp /Cx /Fl /Sa $(DEBUG) /Tamath.s

mdboot.obj: mdboot.c
	cl /AT /Osgcel /c /X $(DEBUG) /Gs /G2 /Fc /I..\..\inc /I..\..\inc\common /I..\..\inc\i86 mdboot.c

# special version for boot partition (temporary, we hope)
mdbootbp.obj: mdbootbp.c
	cl /AT /Osgcel /c /X $(DEBUG) /Gs /G2 /Fc /I..\..\inc /I..\..\inc\common /I..\..\inc\i86 mdbootbp.c

mdboot.exe: callmdb.obj mdboot.obj math.obj ..\..\lib\tinyobj\tiny.lib
	link $(LINKDEBUG) /M /NOD /NOE /PACKC callmdb mdboot math, mdboot.exe, mdboot.map, ..\..\lib\tinyobj\tiny.lib,; 

# special version for boot partition (temporary, we hope)
mdbootbp.exe: callmdbb.obj mdbootbp.obj math.obj ..\..\lib\tinyobj\tiny.lib
	link $(LINKDEBUG) /M /NOD /NOE /PACKC callmdbb mdbootbp math, mdbootbp.exe, mdbootbp.map, ..\..\lib\tinyobj\tiny.lib,; 

..\..\lib\tinyobj\tiny.lib:
	cd ..\..\lib\tinyobj
	$(MAKE) all
	cd ..\..\fd\mdboot

clean:
	del *.obj
	del *.cod
	del *.lst
	del *.map
	del *.exe
