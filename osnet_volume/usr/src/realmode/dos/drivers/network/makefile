#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#
# ident "@(#)makefile	1.14	98/05/29 SMI\n"
# 
#################################################################
#								#
#	This makefile builds the network layer object and all	#
#	network drivers.  Much of the detail is hidden in the	#
#	two included files.					#
#								#
#	If you add a new network driver, add the name of its	#
#	directory to DEBUG_DIRS below if its makefile		#
#	implements a debug target.  Otherwise add it to		#
#	OTHER_DIRS.						#
#								#
#################################################################

DRIVERS=..

!include $(DRIVERS)\makefile.inc

DEBUG_DIRS = pcn
OTHER_DIRS =

all: network.obj network.dbj all.sub
install: network.obj network.dbj install.sub
clean: clean.dir clean.sub
	-if exist makesub.bat del makesub.bat

debug: network.obj network.dbj debug.sub

network.obj network.dbj: network.c $(INC)\rmsc.h $(INC)\rmscnet.h

clean.dir:
	-if exist *.obj del *.obj
	-if exist *.dbj del *.dbj
	-if exist *.cod del *.cod
	-if exist *.pdb del *.pdb

!include $(DRIVERS)\makefile.sub
