#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#
# ident "@(#)makefile	1.7	98/05/29 SMI\n"
# 
#########################################################################
#									#
#	This makefile is a prototype for adding a realmode driver	#
#	for a new device using the generic driver interface.		#
#									#
#	To create a makefile for a new device driver, copy this		#
#	file to the directory where the new driver will be built.	#
#	Replace "proto" in the "DVR=proto" line below by the name of	#
#	the new driver.  If your driver contains a single C source	#
#	file and a single header file, and the filename prefixes	#
#	are the same as the driver name, no other changes are		#
#	necessary.  Otherwise you will need add other files to the	#
#	link list or changes names etc as appropriate.			#
#									#
#	If you edit this file on Solaris, be sure to preserve the	#
#	carriage returns at the end of each existing line and to	#
#	add them to any new lines.  Some versions of the MS-DOS		#
#	linker are unable to parse lines without carriage returns	#
#	correctly.							#
#									#
#	To build the normal (.BEF) version of the driver run "nmake"	#
#	from the new driver directory.  To build a version to run	#
#	with BEFDEBUG (.EXE) run "nmake debug".				#
#									#
#########################################################################

# Name of this driver
DVR=proto

# Import some definitions from a common file
DRIVERS=..
!include $(DRIVERS)\makefile.inc

# Default target: build the regular driver
all: $(DVR).bef
install: install.dir
clean: clean.dir
debug: all $(DVR).exe

# How to install the driver
install.dir: $(DVR).bef
#	For a real driver the line below would be uncommented so that
#	the install target would install the driver.  But we don't want
#	to install the proto driver.
#	copy $(DVR).bef $(DRIVERS)\..\proto\$(DVR).bef

# How to link the driver
$(DVR).bef: $(BCRT0) $(GENOBJS) $(DVR).obj $(LIBS)
	link /nologo /NOD /M @<<link.dat
$(BCRT0) $(GENOBJS) $(DVR).obj
$(DVR).bef,
$(DVR).map,
$(LIBS);
<<

# How to link the debug version of the driver
$(DVR).exe: $(BCRT0) $(DBCRT0) $(DGENOBJS) $(DVR).dbj $(LIBS)
	link /nologo /NOD /M /CODEVIEW @<<link.dat
$(BCRT0) $(DBCRT0) $(DGENOBJS) $(DVR).dbj
$(DVR).exe,
$(DVR).mpd,
$(LIBS);
<<

# Rules/dependencies for building the object modules
$(DVR).obj $(DVR).dbj: $(DVR).c $(DVR).h $(INC)\rmsc.h $(INC)\rmscnet.h

# No installation required for prototype driver
install.dir:

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
