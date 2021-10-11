#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)getaudit.spec	1.2	99/10/14 SMI"
#
# lib/libbsm/spec/getaudit.spec

function	getaudit
include		<sys/param.h>, <bsm/audit.h>
declaration	int getaudit(struct auditinfo *info)
version		SUNW_0.7
errno		EFAULT EPERM EOVERFLOW
exception	$return == -1
end		

function	setaudit
include		<sys/param.h>, <bsm/audit.h>
declaration	int setaudit(struct auditinfo *info)
version		SUNW_0.7
errno		EFAULT EPERM
exception	$return == -1
end		

function	getaudit_addr
include		<sys/param.h>, <bsm/audit.h>
declaration	int getaudit_addr(struct auditinfo_addr *info, int len)
version		SUNW_1.2
errno		EFAULT EPERM
exception	$return == -1
end		

function	setaudit_addr
include		<sys/param.h>, <bsm/audit.h>
declaration	int setaudit_addr(struct auditinfo_addr *info, int len)
version		SUNW_1.2
errno		EFAULT EPERM
exception	$return == -1
end		

