#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

#
#ident "@(#)makefile   1.3   99/09/30 SMI"
#

# Compiler (cl) flags
#
# nologo: suppress logo message
# Alfu: large model with no assumption that stack and data segs are the same
# Gt0: set the data size threshold to zero
# G2: use i286 instruction set
# Gs: turn stack checking off
# Os: compile for small size
# Ob1: only inline functions marked as inline
# Zi: symbol table for CodeView debugger

#
# For codeview debugging enable CODEVIEW and disable OPT
#CODEVIEW =/Zi /D__STDC__
OPT =/Os /Ob1 /D__STDC__

IFLAGS=/I..\common\include /I..\bootconf

# Linker flags
#
# nologo: suppress logo message
# F XXXX: set the stacksize to XXXX (hex bytes)
# Fm: create map file
# Zi: symbol table for CodeView debugger

CFLAGS = /nologo /Gt0 /G2 /Gs $(OPT) $(IFLAGS) /DDEBUG $(CODEVIEW) /c
LFLAGS  = /nologo /F 3000 /Fm $(CODEVIEW)

OBJS =  itup2.obj


install: all 
	copy itup2.exe ..\proto\bootpart\solaris\itup2.exe 

all: itup2.exe

itup2.exe: bop.obj itup2.obj
	set LINK=/seg:200
	cl $(LFLAGS) itup2.obj bop.obj 


itup2.obj: ..\bootconf\itu.h
	cl $(CFLAGS) itup2.c 

bop.obj: 
	cl $(CFLAGS) bop.c

clean:
	del *.obj
	del *.exe
	del *.map



