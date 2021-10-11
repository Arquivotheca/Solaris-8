#########################################################################
#									#
#	Copyright (c) 1997 by Sun Microsystems, Inc.			#
#	All Rights Reserved.						#
#									#
#	@(#)readme.bas	1.6	97/04/07 SMI				#
#									#
#########################################################################

Instructions for installation of Realmode DDK Basic Development Files
=====================================================================

This document assumes that you are familiar with the SunSoft Solaris x86
Realmode Driver white paper and the Realmode Driver External Interface
Specification.

The file BASIC.ZIP contains the files necessary for building realmode
drivers.  It includes object files for the driver frameworks, subroutine
libraries, header files and sample driver source code and makefiles.

Download this file (README.BAS) and BASIC.ZIP and copy them into the
root of your realmode development tree.  The root can be any place that
is convenient for you, but full pathnames in this file are based on the
assumption that you are using a directory called C:\BOOT.  If you use a
different location, just remember to substitute your root directory
wherever you see C:\BOOT.

Extract the files from BASIC.ZIP such that the directory hierarchy is
preserved.  Some extraction tools require an argument to prevent all
the files from being placed in the local directory.  You should have
the following directory hierarchy after extraction:

C:\BOOT
    +--	INC
    |
    +--	DRIVERS
	 +-----	HALFBEFS
	 |
	 +-----	INCLUDE
	 |
	 +-----	LIB
	 |
	 +-----	NETWORK
	 |	 +-----	PCN
	 |
	 +----- PROTO
	 |
	 +-----	SCSI
		 +-----	AHA1540

Once you have installed the files in this manner, consult the Realmode
Driver white paper for details of how to write new realmode drivers.

If you need more detailed information about the realmode driver
framework than is available in the the white paper and BASIC.ZIP,
you should read README.SRC and then download and extract SOURCE.ZIP.
This archive contains the full source of the driver framework.

If you are having difficulty getting your driver to work, you should
read README.DBG and then download and extract DEBUG.ZIP.  This archive
contains debug versions of framework object files and a debug utility
to assist with debugging realmode drivers using MS-DOS debuggers.

Release Notes
=============

The zip files contain improvements to the MAKEFILEs that were made since
the Realmode Driver white paper was updated.  The instructions for
modifying the MAKEFILE in the NETWORK or SCSI directory to add a new
driver are obsolete.  The new instructions are in comments in the
MAKEFILEs themselves.

The Realmode Driver white paper did not describe the use of "Static"
throughout the sample code.  Some MS-DOS debuggers do not automatically
handle static symbols, so we adopted a convention of writing "Static"
in place of "static" where we want to indicate that a variable is
not intended to be used by other modules but we still want to be able
to see it in the debugger.  The file C:\BOOT\DRIVERS\INCLUDE\RMSC.H
contains an overrideable definition setting "Static" to the null string.
We recommend that you use "Static" in your driver where appropriate.
Accidental name collisions can be prevented by choosing variable names
with a prefix indicative of your driver.  WARNING: be careful not to
use "Static" for static variables inside functions; the static behavior
will be lost.

At the time of writing (3/13/97) there is a bug in the Solaris boot
subsystem that prevents resource sharing from working properly.  If you
attempt to use the RES_SHARE capability and encounter problems, please
contact SunSoft via your representative or through the email facility on
the DDK web page.

Microsoft has recently released version 5.0 of Visual C/C++.  This version
of MSVC does not include the 16-bit tools required for building the realmode
drivers.  However we understand that licensees of MSVC Version 5.0 can
obtain a copy of the 16-bit tools from version 4.0 on payment of a small
media/handling fee.  Contact your Microsoft sales representative for details.
