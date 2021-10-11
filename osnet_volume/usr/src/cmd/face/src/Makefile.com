#
#ident	"@(#)Makefile.com	1.3	99/03/30 SMI"
#
# Copyright (c) 1990,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/face/src/Makefile.com
#
# common makefile included for face definitions and rules

# these first two directories are default, see /usr/src/Targetdirs
# only the rootdirs target in /usr/src/Makefile should make them.
#
ROOTOAMBASE=	$(ROOT)/usr/sadm/sysadm
ROOTADDONS=	$(ROOTOAMBASE)/add-ons

# other common directories
ROOTSAVE=	$(ROOT)/usr/sadm/pkg/face/save
ROOTINTF=	$(ROOTSAVE)/intf_install
ROOTADDONSFACE= $(ROOTADDONS)/face
ROOTFACE=	$(ROOTADDONSFACE)/applmgmt/FACE

ROOTVMSYS=	$(ROOT)/usr/vmsys
ROOTOASYS=	$(ROOT)/usr/oasys

ROOTOABIN=	$(ROOTOASYS)/bin
ROOTINFO=	$(ROOTOASYS)/info
ROOTINFOOH=	$(ROOTOASYS)/info/OH
ROOTEXTERN=	$(ROOTOASYS)/info/OH/externals
ROOTSTD=	$(ROOTVMSYS)/standard
ROOTVMBIN=	$(ROOTVMSYS)/bin
ROOTVMLIB=	$(ROOTVMSYS)/lib

DIRMODE= 755

# common installation rules
#
$(ROOTINTF)/% : %
	$(INS.file)

$(ROOTFACE)/% : %
	$(INS.file)

$(ROOTOASYS)/% : oasys/%
	$(INS.file)

$(ROOTOABIN)/% : %
	$(INS.file)

$(ROOTVMSYS)/% : %
	$(INS.file)

$(ROOTVMBIN)/% : %
	$(INS.file)

$(ROOTVMLIB)/% : %
	$(INS.file)

$(ROOTVMSYS) $(ROOTOASYS):
	$(INS.dir)

$(ROOTINFO) $(ROOTOABIN):	$(ROOTOASYS)
	$(INS.dir)

$(ROOTINFOOH): $(ROOTINFO)
	$(INS.dir)

$(ROOTEXTERN): $(ROOTINFOOH)
	$(INS.dir)

$(ROOTSTD) $(ROOTVMBIN) $(ROOTVMLIB): $(ROOTVMSYS)
	$(INS.dir)
