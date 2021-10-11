@echo  off
rem     @(#)esavlb.bat  1.2     96/10/18 SMI^M
rem Copyright (c) 1996 Sun Microsystems, Inc. All rights reserved.

call comment header an Adaptec 284xVL VESA Local Bus adapter. 
echo #
echo #    A hardware conflict exists which causes the probe function of esa
echo #    driver conflict with some pci cards. For this reason the probing 
echo #    of esa driver for existence of Adaptec 284xVL VESA Local Bus adapter
echo #    is disabled by default. This batch file will enable such probing.
call comment footer an Adaptec 284xVL VESA Local Bus adapter. 

call comment warning 

call append solaris.map esavlb.d\solaris.map

call makedir solaris
call append solaris\system.add esavlb.d\system.add

call makedir rc.d
call append rc.d\inst9.sh esavlb.d\inst9.sh

echo THIS MDB ENABLES ESA VL PROBING > modified.mdb

:end
echo End of ESAVLB batch file
