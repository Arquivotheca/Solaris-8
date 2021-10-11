@echo  off
rem 	@(#)unisys.bat	1.11	96/02/08 SMI
rem	Copyright 1995 Sun Microsystems, Inc.
rem	All Rights Reserved.

call comment header a  Unisys U6000/DT2 system. 
call comment modules Drivers excluded include el and elink. 
call comment footer (a Unisys U6000/DT2).

call comment warning 

call append solaris.map unisys.d\solaris.map

call makedir solaris
call append solaris\system.add unisys.d\system.add

call makedir rc.d
call append rc.d\inst9.sh unisys.d\inst9.sh

echo THIS MDB EXCLUDES EL AND ELINK > modified.mdb

renbef elink.bef elink.xxx el.bef el.xxx UNISYS

:end
echo End of UNISYS batch file
