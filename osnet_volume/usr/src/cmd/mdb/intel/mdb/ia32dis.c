/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ia32dis.c	1.1	99/08/11 SMI"

#include "dis.h"

extern mdb_tgt_addr_t ia32dis_ins2str(mdb_disasm_t *, mdb_tgt_t *,
    mdb_tgt_as_t, char *, mdb_tgt_addr_t);

/*ARGSUSED*/
static void
ia32dis_destroy(mdb_disasm_t *dp)
{
	/* Nothing to do here */
}

static const mdb_dis_ops_t ia32dis_ops = {
	ia32dis_destroy, ia32dis_ins2str
};

int
ia32_create(mdb_disasm_t *dp)
{
	dp->dis_name = "ia32";
	dp->dis_desc = "Intel 32-bit disassembler";
	dp->dis_ops = &ia32dis_ops;

	return (0);
}
