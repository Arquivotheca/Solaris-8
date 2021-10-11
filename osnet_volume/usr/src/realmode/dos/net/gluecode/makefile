#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile	1.6	98/05/29 SMI

#rm ..\..\..\download\gluecode.com
#cl /Fm /Fc /c /AS /Gs /G2 /X /DMSC60 ..\..\src\gluecode\gluecode.c
#masm ..\..\src\gluecode\startup.s;
#link /M @link.dat
#copy /b ..\loader\loader.bin+gluecode.exe ..\..\..\download\gluecode.com

install: all ..\..\proto\gluecode.com

..\..\proto\gluecode.com: gluecode.exe ..\loader\loader.bin
	copy /b ..\loader\loader.bin+gluecode.exe ..\..\proto\gluecode.com

all: gluecode.exe

gluecode.exe: gluecode.obj startup.obj
	link /nologo /M gluecode.obj+startup.obj,gluecode.exe;

startup.obj: startup.s
	ml /c /nologo startup.s

gluecode.obj: gluecode.c
	cl /nologo /Fm /Fc /c /AS /Gs /G2 /X /DMSC60 gluecode.c

..\loader\loader.bin:
	cd ..\loader
	$(MAKE) $(MAKEFLAGS) loader.bin
	cd ..\gluecode

clean:
	if exist *.obj del *.obj
	if exist *.cod del *.cod
	if exist *.map del *.map
	if exist *.lst del *.lst
	if exist *.exe del *.exe
