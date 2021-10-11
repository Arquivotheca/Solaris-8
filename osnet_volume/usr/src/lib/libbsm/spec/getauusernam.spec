#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)getauusernam.spec	1.2	99/05/14 SMI"
#
# lib/libbsm/spec/getauusernam.spec

function	getauusernam
include		<sys/param.h>
include		<bsm/libbsm.h>
declaration	struct au_user_ent *getauusernam(char *name)
version		SUNW_0.7
exception	($return == 0)
end

function	getauuserent
include		<sys/param.h>
include		<bsm/libbsm.h>
declaration	struct au_user_ent *getauuserent(void)
version		SUNW_0.7
exception	($return == 0)
end

function	setauuser
include		<sys/param.h>
include		<bsm/libbsm.h>
declaration	void setauuser(void)
version		SUNW_0.7
end

function	endauuser
include		<sys/param.h>
include		<bsm/libbsm.h>
declaration	void endauuser(void)
version		SUNW_0.7
end

function	getauusernam_r
include		<sys/param.h>
include		<bsm/libbsm.h>
declaration	struct au_user_ent *getauusernam_r(au_user_ent_t *u, char *name)
version		SUNW_0.8
exception	($return == 0)
end

function	getauuserent_r
include		<sys/param.h>
include		<bsm/libbsm.h>
declaration	struct au_user_ent *getauuserent_r(au_user_ent_t *u)
version		SUNW_0.8
exception	($return == 0)
end
