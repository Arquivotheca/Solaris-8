#  Copyright (c) 1996 Sun Microsystems, Inc.  All Rights Reserved
#  ident "@(#)makefile	1.13	99/03/23 SMI"

#  Solaris x86 Realmode Drivers (Plug-and-Play versions):
#
#  We build three (count 'em, 3) libraries here:
#
#     $(BLIB)  ...  The *.bef library containing driver-specific stuff
#     $(CLIB)  ...  A truncated C library for use by *.bef files
#     $(OLIB)  ...  A "oldnames" library to shut the linker up!
#
#  The oldnames library isn't really used, but if it's not there the linker
#  complains.  It contains a single module: crt0.obj.  This is the C startup
#  module for realmode drivers and we have to built it anyway, so it doesn't
#  really hurt to stick it in a library.

BLIB = common.lib
CLIB = slibce.lib
OLIB = oldnames.lib

#  The contents of the libraries named above:

BOBJ = callback.obj  biostype.obj  delay.obj    names.obj   peek.obj
BMOD = callback+biostype+delay+names+peek

COBJ = fprintf.obj   fsprintf.obj  ldiv.obj     memcmp.obj   memcpy.obj       \
       memset.obj    printf.obj    putc.obj     qsort.obj    rand.obj         \
       strchr.obj    strcmp.obj    strcpy.obj   strlen.obj   strncpy.obj      \
       vfprintf.obj  ll_shift.obj  us_div.obj	us_mod.obj   ul_mul.obj       \
       int86.obj     atoin.obj     isdigit.obj

CMOD = fprintf+fsprintf+ldiv+memcmp+memcpy+memset+printf+putc+qsort+rand+     \
       strchr+strcmp+strcpy+strlen+strncpy+vfprintf+ll_shift+		      \
       us_div+us_mod+ul_mul+int86+atoin+isdigit

OOBJ = crt0.obj
OMOD = crt0

#  We need a separate list of all modules compiled from external source!
XCOBJ = names.obj
XAOBJ = biostype.obj

#  Source for these libraries comes from two different places, this directory
#  and the realmode common source directory, $(RDIR).  We also need $(CDIR),
#  the directory where the Microsoft C library resides in order to resolve
#  include paths.

CDIR = c:\c700
RDIR = ..\..\..\..\common

#  I prefer UNIX-like names:

CC = cl 
AS = ml

AFLAGS = /nologo /Sn /Zm
CFLAGS = /O /Gs /nologo
#CFLAGS = /Fc /O /Gs /nologo
IFLAGS = -I..\include -I$(RDIR)\include

.SUFFIXES: .c .s .obj

.c.obj:
	$(CC) /c $(CFLAGS) $(IFLAGS) $*.c

.s.obj: 
	$(AS) /c $(AFLAGS) $*.s

#  Default production is to build all 3 libraries:
build: $(OLIB) $(CLIB) $(BLIB)

#  Production rule for external source modules
$(XCOBJ):
	$(CC) /c $(CFLAGS) $(IFLAGS) $(RDIR)\lib\$*.c

$(XAOBJ):
	$(AS) /c $(AFLAGS) $(RDIR)\lib\$*.s

#  C startup routine
$(OLIB): $(OOBJ)
	erase $(OLIB)
	LIB /NOLOGO @<<nul $(OLIB) $(OMOD), nul
<<

#  Stripped-down C library
$(CLIB): $(COBJ)
	erase $(CLIB)
	LIB /NOLOGO @<<clib.rsp
	$(CLIB) $(CMOD), nul
<<

#  Driver (application) library
$(BLIB): $(BOBJ)
	erase $(BLIB)
	LIB /NOLOGO @<<nul $(BLIB) $(BMOD), nul
<<

#  Clean up trash
clean:
	del *.obj
	del *.lib

#  External dependencies must be explicitly noted:

names.obj:    $(RDIR)\lib\names.c
biostype.obj: $(RDIR)\lib\biostype.s
