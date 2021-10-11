#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident "@(#)makefile	1.14     99/09/30 SMI"
#
TMPDIR   = c:\tmp
LIBRARY = lib\tinylib.lib
OBJS =  fat.obj bios.obj dos.obj util.obj chario.obj
#
# /Fc produces COD stuff
# /G2 targets 80286 machine
# /Gs disables stack checking
# /X ignores standard include directory
# /Os compile for size not speed
#   c enable block-level common subexpression opt (default)
#   e ignore register keyword
#   g enable global-level common subexpression opt
#   l enable loop opt.
# The optimizing flags above are the ones that other programs linked
# for real-mode seem to use. Unfortunately, I found that /Oe at least
# causes problems in fatMap().
# Rick McNeal 16-Aug-1994
#CFLAGS = /nologo /AT /c /X /Gs /G2 /Fc

#
#      Flags for debugging
# /Zi line number and symbolic info.
# /Od disable optimizations
# /AT tiny module
# /Fc Source, assembler and machine code listing in .COD file
# /X  don't use default includes
# /c  generate object file
# /Gs disables stack checking
# /DDEBUG add debug code and printfs
#
CFLAGS = /nologo /c /Zi /Od /AT /X /Fc /Gs /G2 /DNEW_CHARIO /DDEBUG
ML= ml
MLFLAGS= /nologo /c /Cp /Cx /Fl /Sa /Zf /Zd /Zi /Sg

install: all ..\proto\bootpart\strap.com ..\proto\bootpart\solaris\strap.rc

..\proto\bootpart\strap.com: strap.com
	copy strap.com ..\proto\bootpart\strap.com

..\proto\bootpart\solaris\strap.rc: strap.rc
	copy strap.rc ..\proto\bootpart\solaris\strap.rc

all: strap.com

$(LIBRARY):
	cd lib
	$(MAKE) all
	cd ..

strap.com: exe2bin.exe strap.exe
	copy exe2bin.exe $(TMPDIR)\exe2bin.exe
	$(TMPDIR)\exe2bin -strip_to_entry strap
	del $(TMPDIR)\exe2bin.exe

strap.exe: $(LIBRARY) asm.obj strap.obj $(OBJS)
	link /NOLOGO /CO /M /NOI /NOD /PACKC @<<strap.lst
asm strap bios fat dos util chario
strap.exe
strap.map
$(LIBRARY) ;
<<

test: call.obj test.obj $(OBJS) forth.obj exe2bin.exe
	link /NOLOGO /M /CO /NOD /NOI @<<test.lst
call test bios fat dos util chario forth.obj
test.exe
test.map
lib\tinylib.lib ;
<<
	copy exe2bin.exe $(TMPDIR)\exe2bin.exe
	$(TMPDIR)\exe2bin -strip_to_entry test
	del $(TMPDIR)\exe2bin.exe

exe2bin.exe: exe2bin.c
	cl exe2bin.c

blastit: call.obj blastit.obj $(OBJS)
	link /NOLOGO /M /CO /NOI /NOD /PACKC @<<blast.lst
call blastit bios fat dos util
blastit.exe
blastit.map
lib\tinylib.lib ;
<<

clean:
	cd lib
	$(MAKE) clean
	cd ..
	if exist *.obj del *.obj
	if exist *.exe del *.exe
	if exist *.map del *.map
	if exist *.cod del *.cod
	if exist *.bin del *.bin
	if exist *.lst del *.lst
	if exist *.pdb del *.pdb
	if exist *.com del *.com

call.obj: call.s
	$(ML) $(MLFLAGS) %s

asm.obj: asm.s
	$(ML) $(MLFLAGS) %s

