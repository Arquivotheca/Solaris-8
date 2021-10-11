#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	99/08/16 SMI"
#
# cmd/perl/Makefile
#

include ../Makefile.cmd

SED=	/usr/bin/sed
TOUCH=	/usr/bin/touch 

BASE_FILES= \
	Policy.sh \
	config.h \
	config.sh \
	installperl \
	writemain

MACH_FILES= $(BASE_FILES:%=$(MACH)/%)

DISTRIB_FILES= $(MACH_FILES:$(MACH)/%=distrib/%)
TESTS:sh = /usr/bin/find distrib/t -name SCCS -prune -o -name \*.t -print


all	:= TARGET = all
install	:= TARGET = install
clobber := TARGET = clobber
clean	:= TARGET = clean
test	:= TARGET = test

#
# The 'test' target is only valid after a successful 'all' build.  Because the
# modules component of the all build is not entirely incremental, it is
# presently inappropriate to make 'test' dependent on 'all'.
#
all install clobber clean test:
	@cd distrib; pwd; $(MAKE) $(TARGET)
	@cd contrib; pwd; $(MAKE) $(TARGET)

install: all

all install test: localize

test: localize_test

clean clobber: remove_localize

localize: $(DISTRIB_FILES) localize_config

remove_localize:
	-$(RM) $(DISTRIB_FILES)

localize_test:
	for i in $(TESTS); do $(CHMOD) 0555 $$i; done
	$(CHMOD) 0555 distrib/t/TEST
	$(CHMOD) 0555 distrib/t/comp/cpp.aux

localize_config:
	$(CHMOD) 0555 distrib/configpm
	$(CHMOD) 0555 distrib/mv-if-diff
	$(CHMOD) 0555 distrib/myconfig

distrib/installperl : $(MACH)/installperl
	$(CP) -f $(MACH)/installperl $@
	$(CHMOD) 0555 $@

distrib/writemain : $(MACH)/writemain
	$(CP) -f $(MACH)/writemain $@
	$(CHMOD) 0555 $@

distrib/config.sh : $(MACH)/config.sh
	$(SED) -e "s@\'cc@\'$(CC)@" $(MACH)/config.sh > $@

distrib/config.h : $(MACH)/config.h $(MACH)/config.sh
	$(SED) -e "s@\'cc@\'$(CC)@" $(MACH)/config.h > $@

distrib/% : $(MACH)/%
	$(CP) -f $< $@
