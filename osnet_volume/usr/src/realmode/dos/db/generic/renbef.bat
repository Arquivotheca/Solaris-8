@echo  off
rem 	@(#)renbef.bat	1.3	95/08/23 SMI
rem	Copyright 1995 Sun Microsystems, Inc.
rem	All Rights Reserved.
rem
rem	Batch file for switching to the diskette containing real mode
rem	drivers (if necessary) and renaming some of them.  This file
rem	assumes that all real mode drivers are on a single diskette and
rem	that it is the only diskette containing MDEXEC.  An optional
rem	final unpaired argument is used as part of a termination message.
rem
rem	call renbef old1 new1 old2 new2 .... [ batname ]
rem
rem	For those who build diskettes using these tools, note that this
rem	file must be present on BOTH diskettes to work properly.
rem

:check1
if exist mdexec goto disk1

	echo.
	echo Please switch to boot diskette #1
	pause
	if exist mdexec goto disk1
	goto check1
:disk1

:loop
if x%1==x goto endloop
if x%2==x goto endloop

	if exist %1 rename %1 %2

	:nofile

	shift
	shift
	goto loop

:endloop

if x%1==x goto nomsg
echo End of %1 batch file
:nomsg
