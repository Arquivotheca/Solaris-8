#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#
# ident "@(#)makefile.inc	1.9	97/10/20 SMI\n"
#
#################################################################
#								#
#	This file contains common definitions used by		#
#	makefiles in this sub-tree.  Makefiles must set		#
#	DRIVERS to this directory before including this		#
#	file.							#
#								#
#	Makefiles may set CFREG or CFDEBUG respectively		#
#	to add extra flags for building .obj and .dbj		#
#	files.  Makefiles can avoid definitions kept		#
#	by not including it.					#
#								#
#################################################################

# Names for directories within the DRIVERS sub-tree
INC=$(DRIVERS)\include
LIB=$(DRIVERS)\lib
NET=$(DRIVERS)\network
SCSI=$(DRIVERS)\scsi

# Name for SunSoft install directory.
INSTALL=$(DRIVERS)\..\proto

# Flag definitions for compilation and assembly
INCDIRS=/I$(INC) /I$(DRIVERS)\..\inc
COMCFLAGS=/c /nologo /AS /Gs /G2 /X $(INCDIRS)
CFLAGS=/O $(CFREG) $(COMCFLAGS)
DCFLAGS=$(CFDEBUG) /DDEBUG /Zi /Fc $(COMCFLAGS)
AFLAGS = /nologo /Cp /Sn /Zm

# Regular and debug object lists for network drivers
NETOBJS=$(DRIVERS)\main.obj $(NET)\network.obj
DNETOBJS=$(DRIVERS)\main.dbj $(NET)\network.dbj

# Regular and debug object lists for SCSI drivers
SCSIOBJS=$(DRIVERS)\main.obj $(SCSI)\scsi.obj
DSCSIOBJS=$(DRIVERS)\main.dbj $(SCSI)\scsi.dbj

# Regular and debug object lists for generic drivers
GENOBJS=$(DRIVERS)\main.obj
DGENOBJS=$(DRIVERS)\main.dbj

# Library files and start-up objects
BLIB=$(LIB)\common.lib
CLIB=$(LIB)\slibce.lib
OLIB=$(LIB)\oldnames.lib
HLIB=$(LIB)\helpers.lib
LIBS=$(BLIB) $(CLIB) $(HLIB)
BCRT0=$(LIB)\crt0.obj
DBCRT0=$(LIB)\dbgcrt0.obj

# Known file types and compilation and assembly rules
.SUFFIXES: .c .s .obj .dbj

.c.obj:
	cl $(CFLAGS) $*.c

.c.dbj:
	cl /Fo$*.dbj $(DCFLAGS) $*.c

.s.obj: 
	ml /c $(AFLAGS) $*.s
