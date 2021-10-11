#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile	1.10	99/09/30 SMI

# Build Solaris MDB executive

TMPDIR   = c:\tmp
# DEBUG=/Zf /Zd /Zi
# LINKDEBUG=/pause /co /inf
DEBUG=
LINKDEBUG=
INCLUDES=/I..\..\inc /I..\..\inc\i86 /I..\..\inc\common
CFLAGS=/nologo /AT /Oscegl /c /X /Gs /G2 /Fc

all: ..\..\proto\mdexec

clean: 
	del *.obj
	del *.lst
	del *.cod
	del *.map
	del *.exe 
	
..\..\proto\mdexec: mdexec.exe
	copy ..\..\exe2bin.exe $(TMPDIR)\exe2bin.exe
	$(TMPDIR)\exe2bin mdexec.exe ..\..\proto\mdexec
	del $(TMPDIR)\exe2bin.exe

callmdx.obj: callmdx.s
	ml /nologo /c /Cp /Cx /Fl /Sa $(DEBUG) /Sg /Tacallmdx.s

mdexec.obj: mdexec.c
	cl $(CFLAGS) /Zp4 $(INCLUDES) mdexec.c

bootblk.obj: bootblk.c
	cl $(CFLAGS) /Zp4 $(INCLUDES) bootblk.c

reboot.obj: reboot.c
	cl $(CFLAGS) /Zp4 $(INCLUDES) reboot.c

devio.obj: devio.c
	cl $(CFLAGS) $(INCLUDES) devio.c

dosfs.obj: dosfs.c
	cl $(CFLAGS) $(INCLUDES) dosfs.c

ata.obj: ata.c
	cl $(CFLAGS) $(INCLUDES) ata.c

OBJ=callmdx.obj mdexec.obj bootblk.obj reboot.obj devio.obj dosfs.obj ata.obj

mdexec.exe: $(OBJ) ..\..\lib\tinyobj\tiny.lib
	link $(LINKDEBUG) /M /NOI /NOD /PACKC @<<mdexec.lnk
$(OBJ)
mdexec.exe
mdexec.map
..\..\lib\tinyobj\tiny.lib slibce.lib;
<<

..\..\lib\tinyobj\tiny.lib:
	cd ..\..\lib\tinyobj
	$(MAKE) all
	cd ..\..\fd\mdexec
