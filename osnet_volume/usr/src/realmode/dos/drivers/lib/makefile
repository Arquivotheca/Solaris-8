#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#
# ident "@(#)makefile	1.7	98/05/29 SMI\n"
#

MSSLIBCE=msvc\slibce.lib

MODS1=anldiv anlmul anlrem anlshl anlshr annaldiv annalmul
MODS2=annalrem annalshl annalshr annauldi annaulre annaulsh
MODS3=anuldiv anulrem anulshr anllrot anlrrot

# In most of the DRIVERS tree the 'debug' targets do the local
# portion of 'all' and then invoke 'nmake debug' for the debug
# subdirectories.  We do not build debug versions of the libraries,
# so in this makefile 'debug' just does the same as 'all'.

install all debug: reglibs helpers

reglibs:
	cd src
	$(MAKE) $(MAKEFLAGS) all
	cd ..

helpers:
	if exist helpers.lib erase helpers.lib
	lib /NOLOGO helpers +crt0.obj;
	if exist $(MSSLIBCE) .\modcopy $(MSSLIBCE) helpers.lib $(MODS1)
	if exist $(MSSLIBCE) .\modcopy $(MSSLIBCE) helpers.lib $(MODS2)
	if exist $(MSSLIBCE) .\modcopy $(MSSLIBCE) helpers.lib $(MODS3)

clean:
	if exist helpers.lib del helpers.lib
	cd src
	$(MAKE) $(MAKEFLAGS) clean
	cd ..
