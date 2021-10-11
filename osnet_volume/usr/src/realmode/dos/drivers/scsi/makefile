#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#
# ident "@(#)makefile	1.13	98/05/29 SMI\n"
#
#################################################################
#								#
#	This makefile builds the SCSI layer object and all	#
#	SCSI drivers.  Much of the detail is hidden in the	#
#	two included files.					#
#								#
#	If you add a new SCSI driver, add the name of its	#
#	directory to DEBUG_DIRS below if its makefile		#
#	implements a debug target.  Otherwise add it to		#
#	OTHER_DIRS.						#
#								#
#################################################################

DRIVERS=..

!include $(DRIVERS)\makefile.inc

DEBUG_DIRS = aha1540
OTHER_DIRS =

all: scsi.obj scsi.dbj all.sub
install: scsi.obj scsi.dbj install.sub
clean: clean.dir clean.sub
	-if exist makesub.bat del makesub.bat

debug: scsi.obj scsi.dbj debug.sub

scsi.obj scsi.dbj: scsi.c $(INC)\rmsc.h $(INC)\rmscscsi.h

clean.dir:
	-if exist *.obj del *.obj
	-if exist *.dbj del *.dbj
	-if exist *.cod del *.cod
	-if exist *.pdb del *.pdb

!include $(DRIVERS)\makefile.sub
