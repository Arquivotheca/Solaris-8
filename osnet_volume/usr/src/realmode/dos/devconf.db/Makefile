#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)Makefile        1.10     99/09/30 SMI

# shell script programs
CMDS            = chkpt dcb ds move proc_pkg

TEMPLATES       = copyright.tmpl    \
		  ident.tmpl

THE_WORKS       = $(CMDS) $(TEMPLATES)

SRC_REL         = $(THE_WORKS)

.KEEP.STATE:

all: get SET_DFLT DCB_UD

#
# Default should be the subdirectory containing the database file
# with the layout we desire for DCB creation.  The default can be
# overridden with dcb -s {layout_subdirectory}
#
DEFAULT_DCB	= single

SET_DFLT:
	ln -s $(DEFAULT_DCB) default

DCB_UD: ../proto $(THE_WORKS)
	./dcb -uci

get:    clean $(THE_WORKS)
	make cmds

cmds: $(CMDS)

src_rel: $(SRC_REL)
	@if [ -d ../Codemgr_wsdata -a -d ../SCCS ] ; then \
	    echo "Make checkpt and ws_list files ..." ; \
	    chkpt .. ; \
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
	rm -f default
	rm -f $(THE_WORKS)
