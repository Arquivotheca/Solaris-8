#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All Rights Reserved.
#
#
# ident "@(#)makefile	1.5	97/04/11 SMI\n"
# 
#########################################################################
#									#
#	This makefile builds the BEFDEBUG realmode driver debug		#
#	utility program.  The program contains several source files	#
#	for driver-related code and one for DOS-related code		#
#	(doswrap.c).  The driver-related objects are built using	#
#	driver include files and doswrap.obj is built using MSVC	#
#	include	files.							#
#									#
#	Most people do not need to use this makefile because an		#
#	executable version of BEFDEBUG is supplied as part of the	#
#	realmode DDK.  This makefile allows you to rebuild BEFDEBUG	#
#	with debugging enabled if you need more detailed information	#
#	than is reported by the distributed version.			#
#									#
#	If your MSVC package is installed somewhere other than in	#
#	C:\MSVC you need to adjust the definition for "MSVC" to		#
#	address the MSVC package directory before using this		#
#	makefile. 							#
#									#
#########################################################################

# Set MSVC to the address of the directory where MSVC is installed.
MSVC=C:\MSVC

# Use DEBUG_FLAGS to add temporary debugging flags, if necessary.
# Set DEBUG_FLAGS back to null when finished debugging.
DEBUG_FLAGS=/DDEBUG /Zi
COMCFLAGS=/c /Alfu /Gs /G2 /X
DBGCFLAGS=$(COMCFLAGS) $(DEBUG_FLAGS)
DRVR_INCLUDE=/I..\include /I..\..\inc /I..\..\common\include
MSVC_INCLUDE=/I$(MSVC)\include

# We build the program for all the standard targets except clean.
# We never install it anywhere else.
install all debug: befdebug.exe dbgdebug.exe

befdebug.exe: befdebug.obj exercise.obj netex.obj node.obj doswrap.obj
	link /nologo /NOD /CODEVIEW @<<link.dat
befdebug.obj exercise.obj netex.obj node.obj doswrap.obj
befdebug,
nul,
$(MSVC)\lib\llibce;
<<

befdebug.obj: befdebug.c befdebug.h
	cl /c $(DRVR_INCLUDE) $(COMCFLAGS) befdebug.c

exercise.obj: exercise.c befdebug.h
	cl /c $(DRVR_INCLUDE) $(COMCFLAGS) exercise.c

node.obj: node.c befdebug.h
	cl /c $(DRVR_INCLUDE) $(COMCFLAGS) node.c

netex.obj: netex.c befdebug.h netex.h
	cl /c $(DRVR_INCLUDE) $(COMCFLAGS) netex.c

doswrap.obj: doswrap.c
	cl /c $(MSVC_INCLUDE) $(COMCFLAGS) doswrap.c

dbgdebug.exe: befdebug.dbj exercise.dbj netex.dbj node.dbj doswrap.dbj
	link /nologo /NOD /CODEVIEW @<<link.dat
befdebug.dbj exercise.dbj netex.dbj node.dbj doswrap.dbj
dbgdebug,
nul,
$(MSVC)\lib\llibce;
<<

befdebug.dbj: befdebug.c befdebug.h
	cl /Fo$@ /c $(DRVR_INCLUDE) $(DBGCFLAGS) befdebug.c

exercise.dbj: exercise.c befdebug.h
	cl /Fo$@ /c $(DRVR_INCLUDE) $(DBGCFLAGS) exercise.c

node.dbj: node.c befdebug.h
	cl /Fo$@ /c $(DRVR_INCLUDE) $(DBGCFLAGS) node.c

netex.dbj: netex.c befdebug.h netex.h
	cl /Fo$@ /c $(DRVR_INCLUDE) $(DBGCFLAGS) netex.c

doswrap.dbj: doswrap.c
	cl /Fo$@ /c $(MSVC_INCLUDE) $(DBGCFLAGS) doswrap.c

clean:
	-if exist *.obj del *.obj
	-if exist *.dbj del *.dbj
