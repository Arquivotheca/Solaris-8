@echo  off
rem 	@(#)intrgrph.bat	1.6	96/02/08 SMI
rem Copyright (c) 1995 Sun Microsystems, Inc. All rights reserved.

call comment header a system from INTERGRAPH with early versions of AMI bios. 
call comment modules Drivers excluded include esa. 
call comment footer (with early AMI pci bios).

call comment warning 

call append solaris.map intrgrph.d\solaris.map

call makedir drv
call replace drv\esa.cnf edj.d\esa.cnf

call makedir solaris
call append solaris\system.add intrgrph.d\system.add

call makedir rc.d
call append rc.d\inst9.sh intrgrph.d\inst9.sh

echo THIS MDB REDUCES ESA > modified.mdb

:end
echo End of INTRGRPH batch file
