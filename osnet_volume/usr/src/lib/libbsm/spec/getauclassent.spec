#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)getauclassent.spec	1.2	99/05/14 SMI"
#
# lib/libbsm/spec/getauclassent.spec

function	getauclassnam
include		<sys/param.h>, <bsm/libbsm.h>
declaration	struct au_class_ent *getauclassnam(char *name)
version		SUNW_0.7
errno		
exception	($return == 0)
end

function	getauclassnam_r
include		<sys/param.h>, <bsm/libbsm.h>
declaration	struct au_class_ent *getauclassnam_r( \
			au_class_ent_t *class_int, char *name)
version		SUNW_0.8
errno		
exception	($return == 0)
end

function	getauclassent
include		<sys/param.h>, <bsm/libbsm.h>
declaration	struct au_class_ent *getauclassent( void)
version		SUNW_0.7
errno		
exception	($return == 0)
end

function	getauclassent_r
include		<sys/param.h>, <bsm/libbsm.h>
declaration	struct au_class_ent *getauclassent_r( \
			au_class_ent_t * class_int)
version		SUNW_0.8
errno		
exception	($return == 0)
end

function	setauclass
include		<sys/param.h>, <bsm/libbsm.h>
declaration	void setauclass(void)
version		SUNW_0.7
errno		
end

function	endauclass
include		<sys/param.h>, <bsm/libbsm.h>
declaration	void endauclass(void)
version		SUNW_0.7
errno		
end
