#
# Copyright (c) 1994 by SunSoft, Inc.
#
# "@(#)%M 1.3     99/07/12 SunSoft"
#
# cmd/print/gateway/translators/Makefile
#

SUBDIRS =	test cascade

all	:= TARGET= all
install	:= TARGET= install
clean	:= TARGET= clean
clobber	:= TARGET= clobber
lint	:= TARGET= lint
strip	:= TARGET= strip
_msg	:= TARGET= catalog

.KEEP_STATE:

all install: $(SUBDIRS)

clean clobber strip cstyle lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
