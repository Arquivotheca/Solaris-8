#
#ident	"@(#)Makefile	1.1	99/01/25 SMI"
#
# Copyright (c) 1996,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/abi/spectrans/Makefile


.KEEP_STATE:

BUILD	=	parser		\
		spec2map	\
		spec2trace	

all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint

all install clean clobber lint: $(BUILD)

_msg: # No messages for these tools

$(BUILD): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

CSDIR	= .
CSPATHS	= $(BUILD:%=$(CSDIR)/%)
CSINCS	= $(CSPATHS:%=-I%)
CSCOPE	= cscope
CTAGS	= ctags
#
# Set CSFLAGS env variable to -bq when using fast cscope to
# build the fast (but large) cscope data bases.
#
CSFLAGS	= -b
CTFLAGS = -wt

.PRECIOUS:	cscope.out

cscope.out: cscope.files FRC
	${CSCOPE} ${CSFLAGS}

cscope.files: FRC
	@-$(RM) cscope.files cscope.files.raw
	echo "$(CSINCS)" > cscope.files
	-find $(CSPATHS) -name SCCS -prune -o \
	    -type d -name '.del-*' -prune -o -type f \
	    \( -name '*.[cshy]' -o -name 'Makefile*' -o -name '*.il' \) \
	    -print > cscope.files.raw
	grep -v Makefile cscope.files.raw >> cscope.files
	grep Makefile cscope.files.raw >> cscope.files
	-$(RM) cscope.files.raw
	@wc -l cscope.files

cscope: cscope.out FRC

FRC:
