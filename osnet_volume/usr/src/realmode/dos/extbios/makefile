#
# Copyright (c) 1993-1995, Sun Microsystems, Inc. All rights reserved.
#
# @(#)makefile        1.6     95/08/31 SMI

install:
	cd common
	$(MAKE) $(MAKEFLAGS) install
	cd ..
	cd cassist
	$(MAKE) $(MAKEFLAGS) install
	cd ..
	cd network
	$(MAKE) $(MAKEFLAGS) install
	cd ..
	cd onepiece
	$(MAKE) $(MAKEFLAGS) install
	cd ..
	cd scsi
	$(MAKE) $(MAKEFLAGS) install
	cd ..

all:
	cd common
	$(MAKE) $(MAKEFLAGS) all
	cd ..
	cd cassist
	$(MAKE) $(MAKEFLAGS) all
	cd ..
	cd network
	$(MAKE) $(MAKEFLAGS) all
	cd ..
	cd onepiece
	$(MAKE) $(MAKEFLAGS) all
	cd ..
	cd scsi
	$(MAKE) $(MAKEFLAGS) all
	cd ..

clean:
	cd common
	$(MAKE) $(MAKEFLAGS) clean
	cd ..
	cd cassist
	$(MAKE) $(MAKEFLAGS) clean
	cd ..
	cd network
	$(MAKE) $(MAKEFLAGS) clean
	cd ..
	cd onepiece
	$(MAKE) $(MAKEFLAGS) clean
	cd ..
	cd scsi
	$(MAKE) $(MAKEFLAGS) clean
	cd ..
