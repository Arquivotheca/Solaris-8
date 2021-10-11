#
#ident	"@(#)Makefile	1.66	99/12/06 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

include $(SRC)/Makefile.master

# These definitions should be moved into an equivalent of ON's Makefile.psm
# located in $(SRC).
PLATFORM:sh=             uname -m
REALMODE_DIR:sh=	 pwd

CMDS	=	drv_mnt

BOOT_DIR	 = $(ROOT)/boot
BOOT_SOL_DIR	 = $(BOOT_DIR)/solaris
BOOT_RCD_DIR	 = $(BOOT_DIR)/rc.d

BOOT_DRV_DIR	 = $(BOOT_SOL_DIR)/drivers
BOOT_DRV_ISA025_DIR	 = $(BOOT_DRV_DIR)/isa.025
BOOT_DRV_ISA125_DIR	 = $(BOOT_DRV_DIR)/isa.125
BOOT_DRV_ISA150_DIR	 = $(BOOT_DRV_DIR)/isa.150
BOOT_DRV_ISA162_DIR	 = $(BOOT_DRV_DIR)/isa.162
BOOT_DRV_ISA175_DIR	 = $(BOOT_DRV_DIR)/isa.175
BOOT_DRV_NOTISA010_DIR = $(BOOT_DRV_DIR)/notisa.010
BOOT_DRV_NOTISA020_DIR = $(BOOT_DRV_DIR)/notisa.020

BOOT_DB_DIR		 = $(BOOT_SOL_DIR)/devicedb
BOOT_MACH_DIR	 = $(BOOT_SOL_DIR)/machines

BOOT_DESTDIR = $(BOOT_DIR) $(BOOT_SOL_DIR) $(BOOT_RCD_DIR) $(BOOT_DRV_DIR) \
	$(BOOT_DRV_ISA025_DIR) $(BOOT_DRV_ISA125_DIR) \
	$(BOOT_DRV_ISA150_DIR) $(BOOT_DRV_ISA162_DIR) \
	$(BOOT_DRV_ISA175_DIR) $(BOOT_DRV_NOTISA010_DIR) \
	$(BOOT_DRV_NOTISA020_DIR) \
	$(BOOT_DB_DIR) $(BOOT_MACH_DIR)

USR_PLAT_DIR		 = $(ROOT)/usr/platform
USR_PSM_DIR		 = $(USR_PLAT_DIR)/$(PLATFORM)
USR_PSM_LIB_DIR		 = $(USR_PSM_DIR)/lib
USR_PSM_LIB_FS_DIR       = $(USR_PSM_LIB_DIR)/fs

USR_LIB_DIR		 = $(ROOT)/usr/lib
USR_LIB_FS_DIR		 = $(USR_LIB_DIR)/fs

#
#  XXX- boot.bin also needs to be here!  It may need to come from a different
#	source, though!
#
BOOT_SOL_FILES = boot.rc bootargs.rc bootconf.exe bootconf.txt \
	bootenv.rc itup2.exe strap.rc

BOOT_RCD_FILES = icdinst9.sh inst9.sh rcs3.sh rcs9.sh

BOOT_DRV_ISA025_FILES = com.bef fdc.bef joyst.bef key.bef \
	logi.bef lpt.bef msm.bef ps2ms.bef
BOOT_DRV_ISA125_FILES = ata.bef dpt.bef \
	pcic.bef  
BOOT_DRV_ISA150_FILES = pcn.bef 
BOOT_DRV_ISA162_FILES = boca.bef
BOOT_DRV_ISA175_FILES = mwss.bef sbpro.bef elx.bef
BOOT_DRV_NOTISA010_FILES = ncrs.bef \
	pcscsi.bef dnet.bef ieef.bef \
	spwr.bef elxl.bef
BOOT_DRV_NOTISA020_FILES = chs.bef iprb.bef

BOOT_DB_FILES = master

COMMON_REALMODE_BOOT = mboot
i386_REALMODE_BOOT = bootblk pboot mboot.cd pboot.cd brvd.cd catalog.cd
sparc_REALMODE_BOOT =
REALMODE_BOOT = $($(MACH)_REALMODE_BOOT) $(COMMON_REALMODE_BOOT)
REALMODE_COM = gluecode.com 
REALMODE_MISC = mdboot mdbootbp strap.com 

REALMODE_SRC_DIR = $(REALMODE_DIR)/dos
REALMODE_PROTO = $(REALMODE_SRC_DIR)/proto
REALMODE_BOOTPART = $(REALMODE_PROTO)/bootpart 
REALMODE_SOL = $(REALMODE_PROTO)/bootpart/solaris
REALMODE_RCD = $(REALMODE_BOOTPART)/rc.d
REALMODE_DB = $(REALMODE_SOL)/devicedb

USR_BOOT_DESTDIR = $(USR_LIB_FS_DIR)/ufs
USR_COM_DESTDIR = $(USR_PSM_LIB_FS_DIR)/nfs
USR_PSM_BOOT_DESTDIR = $(USR_PSM_LIB_FS_DIR)/ufs

INST_BOOT = $(COMMON_REALMODE_BOOT:%=$(USR_BOOT_DESTDIR)/%)	\
	    $($(MACH)_REALMODE_BOOT:%=$(USR_PSM_BOOT_DESTDIR)/%)
INST_COM  = $(REALMODE_COM:%=$(USR_COM_DESTDIR)/%)
INST_SOL  = $(BOOT_SOL_FILES:%=$(BOOT_SOL_DIR)/%)
INST_RCD  = $(BOOT_RCD_FILES:%=$(BOOT_RCD_DIR)/%)
INST_DB   = $(BOOT_DB_FILES:%=$(BOOT_DB_DIR)/%)
INST_ISA25 = $(BOOT_DRV_ISA025_FILES:%=$(BOOT_DRV_ISA025_DIR)/%)
INST_ISA125 = $(BOOT_DRV_ISA125_FILES:%=$(BOOT_DRV_ISA125_DIR)/%)
INST_ISA150 = $(BOOT_DRV_ISA150_FILES:%=$(BOOT_DRV_ISA150_DIR)/%)
INST_ISA162 = $(BOOT_DRV_ISA162_FILES:%=$(BOOT_DRV_ISA162_DIR)/%)
INST_ISA175 = $(BOOT_DRV_ISA175_FILES:%=$(BOOT_DRV_ISA175_DIR)/%)
INST_NOISA10 = $(BOOT_DRV_NOTISA010_FILES:%=$(BOOT_DRV_NOTISA010_DIR)/%)
INST_NOISA20 = $(BOOT_DRV_NOTISA020_FILES:%=$(BOOT_DRV_NOTISA020_DIR)/%)
INST_MISC = $(REALMODE_MISC:%=$(BOOT_DIR)/%)

OWNER=		root
GROUP=		bin

$(INST_BOOT) :=		FILEMODE = 444
$(INST_COM)  :=		FILEMODE = 555

$(USR_LIB_DIR) :=		DIRMODE = 755

all	:=	TARGET= clobber all
install :=	TARGET= clobber install
clean	:=	TARGET= clean
clobber :=	TARGET= clobber

.KEEP_STATE:

all: build_proto

pkg_all:
	@cd pkgdefs; pwd; $(MAKE) all

install: build_proto $(MACH)_install

i386_install: $(USR_PSM_DIR) $(USR_PLAT_DIR) \
	$(USR_BOOT_DESTDIR) $(USR_PSM_BOOT_DESTDIR) \
	$(USR_COM_DESTDIR) $(BOOT_DESTDIR) \
	$(INST_BOOT) $(INST_COM) $(INST_SOL) $(INST_RCD) \
	$(INST_DB) $(INST_ISA25) $(INST_ISA125) \
	$(INST_ISA150) $(INST_ISA162) $(INST_ISA175) \
	$(INST_NOISA10) $(INST_NOISA20) $(INST_MISC)

sparc_install: $(USR_BOOT_DESTDIR) $(INST_BOOT)

clean clobber: build_proto
	@cd pkgdefs; pwd; $(MAKE) $(TARGET)

get: $(CMDS)

build_proto: $(CMDS)
	$(REALMODE_DIR)/drv_mnt $(TARGET)

$(USR_PLAT_DIR):
	$(INS.dir)

$(USR_PSM_DIR): $(USR_PLAT_DIR)
	$(INS.dir)

$(USR_PSM_LIB_DIR):
	$(INS.dir)

$(USR_PSM_LIB_FS_DIR): $(USR_PSM_LIB_DIR)
	$(INS.dir)

$(USR_COM_DESTDIR) $(USR_PSM_BOOT_DESTDIR): $(USR_PSM_LIB_FS_DIR)
	$(INS.dir)

$(USR_BOOT_DESTDIR) $(BOOT_DESTDIR):
	$(INS.dir)

$(BOOT_SOL_DIR)/%:	$(REALMODE_SOL)/%
	$(INS.file)

$(BOOT_RCD_DIR)/%:	$(REALMODE_RCD)/%
	$(INS.file)

$(BOOT_DIR)/%:	$(REALMODE_PROTO)/% 
	$(INS.file)

$(BOOT_DIR)/%:	$(REALMODE_BOOTPART)/%
	$(INS.file)

$(BOOT_DRV_ISA025_DIR)/%:	$(REALMODE_PROTO)/%
	$(INS.file)

$(BOOT_DRV_ISA125_DIR)/%:	$(REALMODE_PROTO)/%
	$(INS.file)

$(BOOT_DRV_ISA150_DIR)/%:	$(REALMODE_PROTO)/%
	$(INS.file)

$(BOOT_DRV_ISA162_DIR)/%:	$(REALMODE_PROTO)/%
	$(INS.file)

$(BOOT_DRV_ISA175_DIR)/%:	$(REALMODE_PROTO)/%
	$(INS.file)

$(BOOT_DRV_NOTISA010_DIR)/%: $(REALMODE_PROTO)/%
	$(INS.file)

$(BOOT_DRV_NOTISA020_DIR)/%: $(REALMODE_PROTO)/%
	$(INS.file)

$(USR_BOOT_DESTDIR)/%:  $(REALMODE_PROTO)/%
	$(INS.file)

$(USR_PSM_BOOT_DESTDIR)/%:  $(REALMODE_PROTO)/%
	$(INS.file)

$(USR_COM_DESTDIR)/%:    $(REALMODE_PROTO)/%
	$(INS.file)
