@echo  off
rem 	@(#)replace.bat	1.3	95/12/11 SMI
rem	Copyright 1995 Sun Microsystems, Inc.
rem	All Rights Reserved.
rem
rem	Replace a destination file by a source file or
rem	just copy if the destination does not exist.  Intended
rem	to be called from other batch files, e.g.:
rem
rem	Synopsis:
rem		replace destination source
rem

if not exist %1 goto copyit

ctty nul
attrib -r %1
ctty con
del %1

:copyit
copy %2 %1
if not exist %1 comment error replace %1 %2 failure. 
