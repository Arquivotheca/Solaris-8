#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#
# ident "@(#)makefile	1.15	98/05/29 SMI\n"
# 
#################################################################
#								#
#	This makefile builds the generic layer object and	#
#	all subdirectories.  Much of the detail is hidden	#
#	in the two included files.				#
#								#
#	If you add a new subdirectory for a driver, a		#
#	half-BEF, or a new layer, add the name of the		#
#	directory to DEBUG_DIRS below if the makefile in	#
#	the new directory supports the debug target.		#
#	Otherwise add the directory name to OTHER_DIRS.		#
#								#
#	The debug target must be present in makefiles		#
#	that are distributed with the realmode DDK.  For	#
#	others it is optional.					#
#								#
#################################################################

DRIVERS=.

!include makefile.inc

DEBUG_DIRS = lib halfbefs proto network scsi
OTHER_DIRS =

all: main.obj main.dbj all.sub
install: main.obj main.dbj install.sub
clean: clean.dir clean.sub
	-if exist makesub.bat del makesub.bat

debug: main.obj main.dbj debug.sub

main.obj main.dbj: main.c $(INC)\rmsc.h

clean.dir:
	-if exist *.obj del *.obj
	-if exist *.dbj del *.dbj
	-if exist *.cod del *.cod
	-if exist *.pdb del *.pdb

!include $(DRIVERS)\makefile.sub
