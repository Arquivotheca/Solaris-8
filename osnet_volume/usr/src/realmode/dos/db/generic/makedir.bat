@echo  off
rem 	@(#)makedir.bat	1.3	95/12/11 SMI
rem	Copyright 1995 Sun Microsystems, Inc.
rem	All Rights Reserved.
rem
rem	Create a directory corresponding to each argument.
rem
rem	Example:
rem		call makedir parent parent\child parent\child\grandchild
rem

rem	Cannot use EXIST to test for existing directory, hide
rem	any error message and ask to create it.

:loop
ctty nul
if x%1==x goto end

	mkdir %1
        echo > %1\tmp
	ctty con
	if not exist %1\tmp comment error mkdir %1 failure. 
	ctty nul
	del %1\tmp

	shift
	goto loop

:end
ctty con
