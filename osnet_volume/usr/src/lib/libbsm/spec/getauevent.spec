#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)getauevent.spec	1.2	99/05/14 SMI"
#
# lib/libbsm/spec/getauevent.spec

function	getauevent
include		<sys/param.h>, <bsm/libbsm.h>
declaration	struct au_event_ent *getauevent(void)
version		SUNW_0.7
exception	($return == 0)
end

function	getauevnam
include		<sys/param.h>, <bsm/libbsm.h>
declaration	struct au_event_ent *getauevnam(char *name)
version		SUNW_0.7
exception	($return == 0)
end

function	getauevnum
include		<sys/param.h>, <bsm/libbsm.h>
declaration	struct au_event_ent *getauevnum(au_event_t event_number)
version		SUNW_0.7
exception	($return == 0)
end

function	getauevnonam
include		<sys/param.h>, <bsm/libbsm.h>
declaration	au_event_t getauevnonam(char *event_name)
version		SUNW_0.7
end

function	setauevent
include		<sys/param.h>, <bsm/libbsm.h>
declaration	void setauevent(void)
version		SUNW_0.7
end

function	endauevent
include		<sys/param.h>, <bsm/libbsm.h>
declaration	void endauevent(void)
version		SUNW_0.7
end

function	getauevent_r
include		<sys/param.h>, <bsm/libbsm.h>
declaration	struct au_event_ent *getauevent_r(au_event_ent_t *e)
version		SUNW_0.8
exception	($return == 0)
end

function	getauevnam_r
include		<sys/param.h>, <bsm/libbsm.h>
declaration	struct au_event_ent *getauevnam_r(au_event_ent_t *e, char *name)
version		SUNW_0.8
end

function	getauevnum_r
include		<sys/param.h>, <bsm/libbsm.h>
declaration	struct au_event_ent *getauevnum_r(au_event_ent_t *e, \
			au_event_t  event_number)
version		SUNW_0.8
exception	($return == 0)
end
