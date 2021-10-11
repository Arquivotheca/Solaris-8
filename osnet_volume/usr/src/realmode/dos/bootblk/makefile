#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile	1.5	99/09/30 SMI

# makefile for hard disk middle-stage boot (bootblk and strap.com)

TMPDIR	 = c:\tmp
INCLUDES=/I..\inc /I..\inc\common /I..\inc\i86
# DEBUG=/DDEBUG
CMNCFLAGS=/nologo /DFARDATA /AS /O /c /X /Zi /Zd /Gs /Zp4
CFLAGS=$(CMNCFLAGS) /G3 /Fc $(INCLUDES) $(DEBUG)
# ufs.c does not use /G3 because it causes compiler errors in optimization
UFSCFLAGS=$(CMNCFLAGS) /G2 /Fc $(INCLUDES) $(DEBUG)
STRAPCFLAGS=$(CMNCFLAGS) /G3 $(INCLUDES) $(DEBUG)
BOBJS=callboot.obj bootblk.obj bootio.obj reboot.obj ix_alts.obj \
	devio.obj ufs.obj
SOBJS=asm.obj strap.obj rcfile.obj bootio.obj reboot.obj dosfs.obj \
	dos.obj bios.obj util.obj chario.obj

install: all ..\proto\bootblk ..\proto\bootpart\strap.com \
	..\proto\bootpart\solaris\strap.rc

..\proto\bootblk: bootblk
        copy bootblk ..\proto\bootblk

..\proto\bootpart\strap.com: strap.com
	copy strap.com ..\proto\bootpart\strap.com

..\proto\bootpart\solaris\strap.rc: strap.rc
	copy strap.rc ..\proto\bootpart\solaris\strap.rc

all: bootblk strap.com

bootblk: bootblk.exe
	copy ..\exe2bin.exe $(TMPDIR)\exe2bin.exe
        $(TMPDIR)\exe2bin -strip_to_entry bootblk.exe bootblk
	del $(TMPDIR)\exe2bin.exe

strap.com: strap.exe
	copy ..\exe2bin.exe $(TMPDIR)\exe2bin.exe
	$(TMPDIR)\exe2bin -strip_to_entry strap.exe strap.com
	del $(TMPDIR)\exe2bin.exe

callboot.obj: callboot.s
        @ml /nologo /c /Cp /Cx /Fl /Sa /Zf /Zd /Zi /Sg /Tacallboot.s

asm.obj: asm.s
        @ml /nologo /c /Cp /Cx /Fl /Sa /Zf /Zd /Zi /Sg /Taasm.s

bootblk.obj: bootblk.c bootblk.h
        @cl $(CFLAGS) /Fm /Fx bootblk.c

strap.obj: bootblk.c bootblk.h
        @cl $(STRAPCFLAGS) /DSTRAP /Fcstrap.cod /Fo$@ /Fm /Fx bootblk.c

rcfile.obj: rcfile.c
        @cl $(CFLAGS) /Fm /Fx rcfile.c

bootio.obj: bootio.c bootblk.h
        @cl $(CFLAGS) /Fm /Fx bootio.c

reboot.obj: reboot.c bootblk.h
        @cl $(CFLAGS) /Fm /Fx reboot.c

ix_alts.obj: ix_alts.c bootblk.h
        @cl /DPRIMARY_BOOT $(CFLAGS) /Fm /Fx ix_alts.c

devio.obj: devio.c
        @cl $(CFLAGS) /I..\inc\stand devio.c

ufs.obj: ufs.c bootblk.h
        @cl $(UFSCFLAGS) /I..\inc\stand ufs.c

dosfs.obj: dosfs.c disk.h
        @cl $(CFLAGS) /DREADONLY /Fm /Fx dosfs.c

dos.obj: dos.c disk.h
        @cl $(CFLAGS) /DREADONLY /Fm /Fx dos.c

bios.obj: bios.c disk.h
        @cl $(CFLAGS) /Fm /Fx bios.c

util.obj: util.c disk.h chario.h varargs.h
        @cl $(CFLAGS) /Fm /Fx util.c

chario.obj: chario.c disk.h chario.h
        @cl $(CFLAGS) /Fm /Fx chario.c

bootblk.exe: $(BOBJS) lib\smalllib.lib
        @link /nologo /M /NOE /NOD @<<bootblk.lnk
$(BOBJS)
bootblk.exe
bootblk.map
..\lib\smallobj\small.lib lib\smalllib.lib;
<<

strap.exe: $(SOBJS) lib\smalllib.lib
        @link /nologo /M /NOE /NOD @<<bootblk.lnk
$(SOBJS)
strap.exe
strap.map
..\lib\smallobj\small.lib lib\smalllib.lib;
<<

lib\smalllib.lib:
	cd lib
	$(MAKE) all
	cd ..

clean:
        if exist bootblk del bootblk
	if exist strap.com del strap.com
        if exist *.obj del *.obj
        if exist *.cod del *.cod
        if exist *.lst del *.lst
        if exist *.bin del *.bin
        if exist *.exe del *.exe
        if exist *.map del *.map
        if exist *.pdb del *.pdb
        cd lib
        $(MAKE) clean
        cd ..
