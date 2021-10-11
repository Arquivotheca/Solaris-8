@echo  off
rem 	@(#)comment.bat	1.1	95/12/11 SMI
rem	Copyright 1995 Sun Microsystems, Inc.
rem	All Rights Reserved.
rem
rem	Print comment header, modules, footer, warnings, and errors
rem	paragraphs. Intended to be called from other batch files, e.g.:
rem
rem	Synopsis:
rem		comment comment_number additional_text
rem

set comm=%1
set args=
:loop
	shift
	if x%1==x goto done
	set args=%args% %1
	goto loop
:done

if %comm%==header goto header
if %comm%==modules goto modules
if %comm%==footer goto footer
if %comm%==warning goto warning
goto error

:header
cls
echo #######################################################################
echo #
echo #    This batch file should be run only if you're going to install 
echo #    onto%args% 
echo #    There are one or more driver conflict(s) which require that 
echo #    drivers and/or system files be changed, remapped or disabled 
echo #    (see the Release Notes for details). 
echo #
goto end

:modules 
echo #    This batch file renames realmode driver(s) and adds script(s) 
echo #    which run after the system has been installed to remove 
echo #    the equivalent Solaris drivers from the newly installed root 
echo #    filesystem. %args% 
echo #
goto end

:footer
echo #    These changes are permanent and cannot be undone.  Do not run 
echo #    this batch file unless you are going to install onto a system 
echo #    of the type described above%args% 
echo #    If you run this batch file in error it will remove important 
echo #    drivers and then you may need to obtain replacement boot 
echo #    diskette floppies from Solaris x86 Support 
echo #    (+1.310.348.6070) or from your local support contact. 
echo #
echo #######################################################################
pause
goto end

:warning
if not exist modified.mdb goto end
echo #######################################################################
echo #
echo #     WARNING: THIS BOOT DISKETTE HAS ALREADY BEEN MODIFIED
echo #     A batch file has already been run on this floppy. This may
echo #     lead to unexpected failures and problems in the Solaris boot.
echo #     This step should be performed only in exceptional circumstances,
echo #     when directed by the Release Notes.
echo #
echo #######################################################################
pause
goto end

:error
echo #######################################################################
echo #
echo #     ERROR: MODIFICATION OF THIS BOOT DISKETTE HAS FAILED
echo #     A failure has occurred while modifying this boot floppy.
echo #     Please begin again with a new copy of the diskette(s), and ensure
echo #     that the attrib.exe program is on your current dos path.
if x%args%x==xx goto skip
echo #
echo #     %args%
:skip
echo #
echo #######################################################################
exit

:end
