#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)Makefile	1.31	96/02/08 SMI

# shell script programs
CMDS            = drvud mdb move ds chkpt calc_psr

# C programs
C_PGMS          = patch_bootstrap   \
                  sp_req

TEMPLATES       = copyright.tmpl    \
		  ident.tmpl

DB_FILES        = database          \
		  du_env

INST_SCRIPTS    = installdu.sh

SYSTEM_FILES    = generic/rc.d/inst9.sh         \
                  generic/rc.d/rcs9.sh         \
		  generic/solaris.map           \
		  generic/append.bat            \
		  generic/comment.bat           \
		  generic/makedir.bat           \
		  generic/renbef.bat            \
		  generic/replace.bat           \
		  generic/solaris/classes.txt   \
		  generic/solaris/nam_maj.txt   \
		  generic/solaris/system.add    \
		  nov2000/nov2000.bat           \
		  nov2000/nov2000.d/inst9.sh    \
		  nov2000/nov2000.d/solaris.map \
		  nov2000/nov2000.d/system.add	\
		  unisys/unisys.bat             \
		  unisys/unisys.d/inst9.sh      \
		  unisys/unisys.d/solaris.map   \
		  unisys/unisys.d/system.add    \
		  pcnet/pcnet.bat               \
		  pcnet/pcnet.d/inst9.sh        \
		  pcnet/pcnet.d/solaris.map     \
		  pcnet/pcnet.d/system.add      \
		  edj/edj.bat           \
		  edj/edj.d/esa.cnf     \
		  edj/edj.d/inst9.sh    \
		  edj/edj.d/solaris.map \
		  edj/edj.d/system.add  \
		  intrgrph/intrgrph.bat         \
		  intrgrph/intrgrph.d/inst9.sh  \
		  intrgrph/intrgrph.d/solaris.map \
		  intrgrph/intrgrph.d/system.add  \
		  esavlb/esavlb.bat         \
		  esavlb/esavlb.d/inst9.sh  \
		  esavlb/esavlb.d/solaris.map \
		  esavlb/esavlb.d/system.add

INFO_FILES      = README

THE_WORKS       = $(CMDS) $(TEMPLATES) $(DB_FILES) $(INST_SCRIPTS) \
		  $(SYSTEM_FILES) $(INFO_FILES) $(C_PGMS)

SRC_REL         = $(THE_WORKS)

.KEEP.STATE:

DRV_UD: $(THE_WORKS)
	drvud -u
	@echo "Make the 3.5" and 5.25" diskettes with 'drvud -d <drive_number>,<diskette_size>'".
	-drvud usage

MDB_UD: ../proto $(THE_WORKS)
	mdb -u
	@echo "Make the 3.5" and 5.25" diskettes with 'mdb -d <drive_number>'".
	-mdb usage

all: get DRV_UD MDB_UD

get:    clean $(THE_WORKS)
	make cmds

cmds: $(CMDS) $(C_PGMS)

patch_bootstrap: patch_bootstrap.c
	cc patch_bootstrap.c -o patch_bootstrap

sp_req: sp_req.c
	cc sp_req.c -o sp_req

src_rel: $(SRC_REL)
	@if [ -d ../Codemgr_wsdata -a -d ../SCCS ] ; then \
	    echo "Make checkpt and ws_list files ..." ; \
	    chkpt mdb ; \
	fi
	@echo "Make source release cpio archive(s) ..."
	@egrep -v "SCCS|Codemgr_wsdata" ws_list > ws_list_clear
	@( cd .. ; \
	    sed '/#ident/d' db/ws_list_clear | \
		cpio -ocm > clear_x86_rm ; compress -f clear_x86_rm ;)
	@rm -f ws_list_clear
	@if [ -d ../Codemgr_wsdata -a -d ../SCCS ] ; then \
	    ( cd .. ; sed '/#ident/d' db/ws_list | \
		cpio -ocm > sccs_x86_rm ; compress -f sccs_x86_rm ;) ; \
	fi

clean:
	rm -f $(THE_WORKS)
