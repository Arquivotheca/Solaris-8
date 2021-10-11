/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sparcdis.c	1.1	99/08/11 SMI"

#include "dis.h"

extern mdb_tgt_addr_t sparcdis_ins2str(mdb_disasm_t *, mdb_tgt_t *,
    mdb_tgt_as_t, char *, mdb_tgt_addr_t);

/*ARGSUSED*/
static void
sparcdis_destroy(mdb_disasm_t *dp)
{
	/* Nothing to do here */
}

static const mdb_dis_ops_t sparcdis_ops = {
	sparcdis_destroy, sparcdis_ins2str
};

static int
sparcdis_create(mdb_disasm_t *dp, const char *name, int flags)
{
	dp->dis_name = name;
	dp->dis_ops = &sparcdis_ops;
	dp->dis_data = (void *)flags;

	if (flags & V9_MODE) {
		if (flags & V9_SGI_MODE)
			dp->dis_desc = "UltraSPARC1-v9 disassembler";
		else
			dp->dis_desc = "SPARC-v9 disassembler";
	} else
		dp->dis_desc = "SPARC-v8 disassembler";

	return (0);
}

int
sparc1_create(mdb_disasm_t *dp)
{
	return (sparcdis_create(dp, "1", V8_MODE));
}

int
sparc2_create(mdb_disasm_t *dp)
{
	return (sparcdis_create(dp, "2", V9_MODE));
}

int
sparc4_create(mdb_disasm_t *dp)
{
	return (sparcdis_create(dp, "4", V9_MODE | V9_SGI_MODE));
}

int
sparcv8_create(mdb_disasm_t *dp)
{
	return (sparcdis_create(dp, "v8", V8_MODE));
}

int
sparcv9_create(mdb_disasm_t *dp)
{
	return (sparcdis_create(dp, "v9", V9_MODE));
}

int
sparcv9plus_create(mdb_disasm_t *dp)
{
	return (sparcdis_create(dp, "v9plus", V9_MODE | V9_SGI_MODE));
}
