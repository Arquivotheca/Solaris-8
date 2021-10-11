#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile	1.29	99/12/06 SMI

PROTO=proto
PARTNPROTO=$(PROTO)\bootpart
SOLARPROTO=$(PARTNPROTO)\solaris
RCDPROTO=$(PARTNPROTO)\rc.d

PROTODIRS=$(PROTO) $(PARTNPROTO) $(SOLARPROTO) $(RCDPROTO) \
    $(SOLARPROTO)\drivers $(SOLARPROTO)\devicedb

SUBDIRS=lib hd fd net bootblk bootconf cdrom extbios drivers itu

install: all subsins rcs rcdshs confdb
all: $(PROTODIRS) exe2bin.exe subsall

$(PROTO):
	-md $@
$(PARTNPROTO):
	-md $@
$(SOLARPROTO):
	-md $@
$(RCDPROTO):
	-md $@
$(SOLARPROTO)\drivers:
	-md $@
$(SOLARPROTO)\devicedb:
	-md $@

exe2bin.exe: exe2bin.c
	@cl /AS /nologo exe2bin.c

subsins: $(SUBDIRS)
	!makesub.bat $** install

subsall: $(SUBDIRS)
	!makesub.bat $** all

subscln: $(SUBDIRS)
	!makesub.bat $** clean

rcs: rcfiles

rcfiles: $(SOLARPROTO)\bootenv.rc $(SOLARPROTO)\bootargs.rc $(SOLARPROTO)\boot.rc

$(SOLARPROTO)\bootenv.rc: devconf.db\package\bootenv.rc
	copy $** $@
$(SOLARPROTO)\bootargs.rc: devconf.db\generic\bootargs.rc
	copy $** $@
$(SOLARPROTO)\boot.rc: devconf.db\generic\boot.rc
	copy $** $@

rcdshs: rcdshfiles

rcdshfiles:	\
	$(RCDPROTO)\icdinst9.sh	\
	$(RCDPROTO)\inst9.sh	\
	$(RCDPROTO)\rcs3.sh	\
	$(RCDPROTO)\rcs9.sh

$(RCDPROTO)\icdinst9.sh: devconf.db\generic\icdinst9.sh
	copy $** $@
$(RCDPROTO)\inst9.sh: devconf.db\generic\inst9.sh
	copy $** $@
$(RCDPROTO)\rcs3.sh: devconf.db\generic\rcs3.sh
	copy $** $@
$(RCDPROTO)\rcs9.sh: devconf.db\generic\rcs9.sh
	copy $** $@

confdb:
	cd devicedb
	!nmake -f makefile.dos install
	cd ..

confcln:
	cd devicedb
	!make.bat clean
	cd ..

clobber: clean
	del /f /q $(SOLARPROTO)\drivers
	rd $(SOLARPROTO)\drivers
	del /f /q $(SOLARPROTO)\devicedb
	rd $(SOLARPROTO)\devicedb
	del /f /q $(SOLARPROTO)
	rd $(SOLARPROTO)
	del /f /q $(PARTNPROTO)
	rd $(PARTNPROTO)
	del /f /q $(PROTO)
	rd $(PROTO)

clean: subscln confcln
	-del exe2bin.exe
	-del exe2bin.obj

