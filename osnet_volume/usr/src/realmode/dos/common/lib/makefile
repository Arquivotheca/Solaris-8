#  Copyright (c) 1995 Sun Microsystems, Inc.  All Rights Reserved
#  ident "@(#)makefile	1.14	95/04/20 SMI"

OBJ =  biostype.obj befpath.obj  devdb.obj    gettext.obj  fprintf.obj \
       fsprintf.obj menu.obj     names.obj    parsemen.obj printf.obj  \
       readdir.obj  reslist.obj  resfree.obj  resmgmt.obj  sprintf.obj \
       vfprintf.obj vprintf.obj  vsprintf.obj

MOD =  biostype    +befpath     +devdb       +gettext     +fprintf     \
      +fsprintf    +menu        +names       +parsemen    +printf      \
      +readdir     +reslist     +resfree     +resmgmt     +sprintf     \
      +vfprintf    +vprintf     +vsprintf

#  Bef loader:
#
#    Use "befext" for the real driver loader, "befake" for the DOS simulator.

BEF = befext
#BEF = befake

CC = cl
AS = ml
LIB = common.lib

CFLAGS = /nologo /AM /O /G2 -I..\include
#CFLAGS = /Zi /nologo /AM /O /G2 -I..\include -DDEBUG 
AFLAGS = /nologo /Zm /Sn

.SUFFIXES: .c .s .obj

.c.obj:
	$(CC) /c $(CFLAGS) $*.c

.s.obj:
	$(AS) /c $(AFLAGS) $*.s

build: $(LIB)

$(LIB): $(OBJ) $(BEF).obj
	erase $(LIB)
	LIB /NOLOGO @<<common.rsp
	$(LIB) $(MOD) + $(BEF), nul
<<

clean:
	erase *.obj
	erase *.cod
	erase *.pdb
	erase $(LIB)
