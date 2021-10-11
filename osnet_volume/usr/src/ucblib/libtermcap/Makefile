#
#ident	"@(#)Makefile	1.7	99/09/21 SMI"
#
# Copyright (c) 1989-1997 by Sun Microsystems, Inc.
# All Rights Reserved.
#
# ucblib/libtermcap/Makefile
#
# The builds occur in isa subdirectories.
#

include $(SRC)/Makefile.master

SUBDIRS = spec .WAIT $(MACH) $(BUILD64) $(MACH64)

# conditional assignments
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

.KEEP_STATE:

all install clean clobber lint : $(SUBDIRS)

$(MACH) $(MACH64) spec: FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

#
# The cscope.out file is made in the current directory and spans all
# supported architectures.
#
CSCOPE			= cscope
CTAGS			= ctags
CSDIR			= .
CSINCS			= -I$(ROOT)/usr/ucbinclude -I$(ROOT)/usr/include
CSPATHS			= $(CSDIR)

.PRECIOUS:	cscope.out

cscope.out: cscope.files FRC
	${CSCOPE} -b -f `pwd`/cscope.out

cscope.files: FRC
	@-$(RM) cscope.files
	echo "$(CSINCS)" > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
	    -type d -name '.del-*' -prune -o -type f \
	    \( -name '*.[csh]' -o -name 'Makefile*' \) \
	    -print >> cscope.files
	echo "llib-ltermcap" >> cscope.files
	echo "../../Makefile.master" >> cscope.files
	echo "../../Makefile.master.64" >> cscope.files
	echo "../../lib/Makefile.lib" >> cscope.files
	echo "../../lib/Makefile.lib.64" >> cscope.files
	echo "../../lib/Makefile.targ" >> cscope.files
	@wc -l cscope.files

tags: tags.list FRC
	${CTAGS} -wt `cat tags.list`

tags.list: cscope.files
	@$(RM) tags.list
	grep '\.c$$' cscope.files > tags.list
FRC:

