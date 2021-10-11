/*
 * Copyright (c) 1996 Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)psetsys.c	1.2	96/11/13 SMI"

/*LINTLIBRARY*/
#include "synonyms.h"
#include <sys/types.h>
#include <sys/procset.h>
#include <sys/processor.h>
#include <sys/pset.h>

#pragma weak pset_create = _pset_create
#pragma weak pset_destroy = _pset_destroy
#pragma weak pset_assign = _pset_assign
#pragma weak pset_info = _pset_info
#pragma weak pset_bind = _pset_bind

int _pset(int, ...);

/* subcode wrappers for _pset system call */

int
pset_create(psetid_t *npset)
{
	return (_pset(PSET_CREATE, npset));
}

int
pset_destroy(psetid_t pset)
{
	return (_pset(PSET_DESTROY, pset));
}

int
pset_assign(psetid_t pset, processorid_t cpu, psetid_t *opset)
{
	return (_pset(PSET_ASSIGN, pset, cpu, opset));
}

int
pset_info(psetid_t pset, int *type, u_int *numcpus, processorid_t *cpulist)
{
	return (_pset(PSET_INFO, pset, type, numcpus, cpulist));
}

int
pset_bind(psetid_t pset, idtype_t idtype, id_t id, psetid_t *opset)
{
	return (_pset(PSET_BIND, pset, idtype, id, opset));
}
