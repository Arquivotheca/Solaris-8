@echo  off
rem 	@(#)edj.bat	1.7	96/02/08 SMI
rem Copyright (c) 1995 Sun Microsystems, Inc. All rights reserved.

call comment header a system from EDJ with early versions of AMI pci bios. 
call comment modules Drivers excluded include esa, and pcplusmp. 
call comment footer (with early AMI pci bios).

call comment warning 

call append solaris.map edj.d\solaris.map

call makedir drv
call replace drv\esa.cnf edj.d\esa.cnf

call makedir solaris
call append solaris\system.add edj.d\system.add

call makedir rc.d
call append rc.d\inst9.sh edj.d\inst9.sh

echo THIS MDB EXCLUDES PCPLUSMP AND REDUCES ESA > modified.mdb

:end
echo End of EDJ batch file
