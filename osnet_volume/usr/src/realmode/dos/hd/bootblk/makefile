#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile	1.11	99/09/30 SMI

# makefile for hard disk middle-stage boot (bootblk)

TMPDIR   = c:\tmp
INCLUDES=/I..\..\inc /I..\..\inc\common /I..\..\inc\i86
CFLAGS=/DFARDATA /nologo /AS /O /c /X /Zi /Zd /Gs /G2 /Fc /Zp4 $(INCLUDES)
OBJS=callboot.obj bootblk.obj reboot.obj ix_alts.obj devio.obj ufs.obj
SLICEOBJS=callboot.obj bootslic.obj devio.obj ufsslice.obj

install: all ..\..\proto\bootblk

..\..\proto\bootblk: bootblk.exe
	copy ..\..\exe2bin.exe $(TMPDIR)\exe2bin.exe
	$(TMPDIR)\exe2bin -strip_to_entry bootblk.exe bootblk
	del $(TMPDIR)\exe2bin.exe
	copy bootblk ..\..\proto\bootblk

all: bootblk.exe

callboot.obj: callboot.s
	@ml /DFARDATA= /nologo /c /Cp /Cx /Fl /Sa /Zf /Zd /Zi /Sg /Tacallboot.s

bootblk.obj: bootblk.c
	@cl $(CFLAGS) /Fm /Fx bootblk.c

reboot.obj: reboot.c
	@cl $(CFLAGS) /Fm /Fx reboot.c

ix_alts.obj: ix_alts.c
	@cl /DPRIMARY_BOOT $(CFLAGS) /Fm /Fx ix_alts.c

devio.obj: devio.c
	@cl $(CFLAGS) /I..\..\inc\stand devio.c

ufs.obj: ufs.c
	@cl $(CFLAGS) /I..\..\inc\stand ufs.c

bootblk.exe: $(OBJS)
	@link /nologo /M /NOE @<<bootblk.lnk
callboot bootblk reboot ix_alts devio ufs
bootblk.exe
bootblk.map
..\..\lib\smallobj\small.lib slibce.lib;
<<

# "slice" make (mkslice.bat)

bootslic.obj: bootblk.c
	@cl /DBSLICE /DFARDATA /AS /Od /c /X /Zi /Gs /G2 /Zd /Fc /Zp4 /I..\..\inc /I..\..\inc\common /I..\..\inc\i86 bootblk.c

ufsslice.obj: ufs.c
	@cl /DFARDATA /DBSLICE /AS /Ocs /c /X /Zi /Gs /G2 /Zd /Fc /Zp4 /I..\..\inc /I..\..\inc\i86 /I..\..\inc\common /I..\..\inc\stand ufs.c

bootslic.exe: $(OBJS)
	@link /inf /co /M /NOE @<<bootslic.lnk
callboot bootslic devio ufsslice
bootslic.exe
bootblk.map
..\..\lib\smallobj\small.lib slibce.lib;
<<

clean:
	if exist bootblk del bootblk
	if exist *.obj del *.obj
	if exist *.cod del *.cod
	if exist *.lst del *.lst
	if exist *.bin del *.bin
	if exist *.exe del *.exe
	if exist *.map del *.map
	if exist *.pdb del *.pdb
