#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#
# ident "@(#)makefile	1.5	98/05/29 SMI\n"
#
#########################################################################
#									#
#	This makefile is for building the realmode probe-only		#
#	drivers for non-bootable devices.				#
#									#
#	To add a new probe-only driver, create the new C source		#
#	file (and header file if required) in this directory.  Add	#
#	the .BEF version of the driver name to the definition of	#
#	DVRS below.  If you want a debug version for 'nmake debug',	#
#	add the .EXE version of the driver name to DBGS below too.	#
#									#
#	If you edit this file on Solaris, be sure to preserve the	#
#	carriage returns at the end of each existing line and to	#
#	add them to any new lines.  Some versions of the MS-DOS		#
#	linker are unable to parse lines without carriage returns	#
#	correctly.							#
#									#
#	To build the normal (.BEF) version of the driver run "nmake"	#
#	from this directory.  To build a version to run	with		#
#	BEFDEBUG (.EXE) run "nmake debug".				#
#									#
#########################################################################

# Drivers to build in this directory
DVRS=joyst.bef
DBGS=joyst.exe

# Import some definitions from a common file
DRIVERS=..
!include $(DRIVERS)\makefile.inc

.SUFFIXES: .bef .c

# How to build a regular driver
.c.bef:
	cl $(CFLAGS) $*.c
	link /nologo /NOD /M @<<link.dat
$(BCRT0) $(GENOBJS) $*.obj
$*.bef,
$*.map,
$(LIBS);
<<

# How to build a debug driver
.c.exe:
	cl /Fo$*.dbj $(DCFLAGS) $*.c
	link /nologo /NOD /M @<<link.dat
$(BCRT0) $(DBCRT0) $(DGENOBJS) $*.dbj
$*.exe,
$*.mpd,
$(LIBS);
<<

# Default target: build the drivers
all: all.dir
install: all.dir install.dir
clean: clean.dir
debug: all $(DBGS)

# How to install the drivers (SunSoft only)
install.dir: $(DVRS)
	!copy $** $(INSTALL)\$**

all.dir: $(DVRS)

# Target for removing intermediate and debug files
clean.dir:
	-if exist *.obj del *.obj
	-if exist *.dbj del *.dbj
	-if exist *.cod del *.cod
	-if exist *.exe del *.exe
	-if exist *.pdb del *.pdb
	-if exist *.map del *.map
	-if exist *.mpd del *.mpd
	-if exist *.bef del *.bef
