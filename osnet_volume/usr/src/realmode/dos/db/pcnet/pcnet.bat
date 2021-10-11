@echo  off
rem 	@(#)pcnet.bat	1.8	96/02/08 SMI
rem	Copyright 1995 Sun Microsystems, Inc.
rem	All Rights Reserved.

call comment header an AMD PC/Net-ISA, (C-)LANCE, or Novell NE2100 ethercard. 
call comment modules Drivers excluded include elink. 
call comment footer (with PCNet type adapter(s)).

call comment warning 

call append solaris.map pcnet.d\solaris.map

call makedir solaris
call append solaris\system.add pcnet.d\system.add

call makedir rc.d
call append rc.d\inst9.sh pcnet.d\inst9.sh

echo THIS MDB EXCLUDES ELINK > modified.mdb

renbef elink.bef elink.xxx PCNET

:end
echo End of PCNET batch file
