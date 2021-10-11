@echo  off
rem 	@(#)nov2000.bat	1.11	96/09/25 SMI
rem Copyright (c) 1996 Sun Microsystems, Inc. All rights reserved.

call comment header a system  with the Novell 2000 or 2000+ ethercard(s). 
call comment modules Drivers excluded: eepro, el, elink, iee, smc, fmvel, pcn 
call comment footer (with Novell 2000 or 2000+ ethercard(s)).

call comment warning 

call append solaris.map nov2000.d\solaris.map

call makedir solaris
call append solaris\system.add nov2000.d\system.add

call makedir rc.d
call append rc.d\inst9.sh nov2000.d\inst9.sh

echo THIS MDB EXCLUDES EEPRO EL ELINK IEE SMC PCN FMVEL > modified.mdb

renbef nei.xxx nei.bef elink.bef elink.xxx el.bef el.xxx iee.bef iee.xxx eepro.bef eepro.xxx pcn.bef pcn.xxx smc.bef smc.xxx fmvel.bef fmvel.xxx NOV2000

:end
echo End of NOV2000 batch file
