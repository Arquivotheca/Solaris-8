/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)scsi_data.c	1.21	96/06/07 SMI"

/*
 * Global SCSI data
 */

#include <sys/scsi/scsi.h>

char *sense_keys[NUM_SENSE_KEYS + NUM_IMPL_SENSE_KEYS] = {
	"No Additional Sense",		/* 0x00 */
	"Soft Error",			/* 0x01 */
	"Not Ready",			/* 0x02 */
	"Media Error",			/* 0x03 */
	"Hardware Error",		/* 0x04 */
	"Illegal Request",		/* 0x05 */
	"Unit Attention",		/* 0x06 */
	"Write Protected",		/* 0x07 */
	"Blank Check",			/* 0x08 */
	"Vendor Unique",		/* 0x09 */
	"Copy Aborted",			/* 0x0a */
	"Aborted Command",		/* 0x0b */
	"Equal Error",			/* 0x0c */
	"Volume Overflow",		/* 0x0d */
	"Miscompare Error",		/* 0x0e */
	"Reserved",			/* 0x0f */
	"fatal",			/* 0x10 */
	"timeout",			/* 0x11 */
	"EOF",				/* 0x12 */
	"EOT",				/* 0x13 */
	"length error",			/* 0x14 */
	"BOT",				/* 0x15 */
	"wrong tape media"		/* 0x16 */
};


char *scsi_state_bits = "\20\05STS\04XFER\03CMD\02SEL\01ARB";


/*
 * This structure is used to allow you to quickly determine the size of the
 * cdb by examining the cmd code.  It is used in conjunction with the
 * CDB_GROUPID macro.  Lookup returns size of cdb.  If unknown, zero returned.
 */
u_char scsi_cdb_size[] = {
	CDB_GROUP0,	/* Group 0, 6  byte cdb */
	CDB_GROUP1,	/* Group 1, 10 byte cdb */
	CDB_GROUP2,	/* Group 2, 10 byte cdb */
	CDB_GROUP3,	/* Group 3,  reserved */
	CDB_GROUP4,	/* Group 4, 16 byte cdb */
	CDB_GROUP5,	/* Group 5, 12 byte cdb */
	CDB_GROUP6,	/* Group 6,  ? byte cdb (vendor specific) */
	CDB_GROUP7	/* Group 7,  ? byte cdb (vendor specific) */
};

/*
 * Basic SCSI command description strings that can be used by drivers
 * to pass to scsi_errmsg().
 */
struct scsi_key_strings scsi_cmds[] = {
	0x00, "test unit ready",
	0x01, "rezero/rewind",
	0x03, "request sense",
	0x04, "format",
	0x07, "reassign",
	0x08, "read",
	0x0a, "write",
	0x0b, "seek",
	0x10, "write file mark",
	0x11, "space",
	0x12, "inquiry",
	0x15, "mode select",
	0x16, "reserve",
	0x17, "release",
	0x18, "copy",
	0x19, "erase tape",
	0x1a, "mode sense",
	0x1b, "load/start/stop",
	0x1e, "door lock",
	0x28, "read(10)",
	0x2a, "write(10)",
	0x2f, "verify",
	0x37, "read defect data",
	0x3b, "write buffer",
	-1, NULL
};
