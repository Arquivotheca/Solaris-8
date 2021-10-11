@echo  off
rem 	@(#)append.bat	1.3	95/12/11 SMI
rem	Copyright 1995 Sun Microsystems, Inc.
rem	All Rights Reserved.
rem
rem 	Synopsis:
rem		append destination source
rem
rem	Example:
rem		call append solaris.map corvette.d\solaris.map
rem

if exist %1 goto append

copy %2 %1
if not exist %1 comment error append %1 %2 failure. 
goto done

:append
ctty nul
attrib -r %1
ctty con
copy /b %1 + %2 %1

:done
