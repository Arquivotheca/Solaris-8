@echo  off
rem 	@(#)corvette.bat	1.8	96/02/08 SMI
rem	Copyright 1995 Sun Microsystems, Inc.
rem	All Rights Reserved.

call comment header a system with the IBM SCSI-2 Fast/Wide Adapter/A. 
call comment modules Drivers excluded include mcis. 
call comment footer (with IBM SCSI-2 Fast/Wide Adapter/A).

call comment warning 

call append solaris.map corvette.d\solaris.map

call makedir solaris
call append solaris\system.add corvette.d\system.add

call makedir rc.d
call append rc.d\inst9.sh corvette.d\inst9.sh

echo THIS MDB EXCLUDES MCIS > modified.mdb

renbef mcis.bef mcis.xxx CORVETTE

:end
echo End of CORVETTE batch file
