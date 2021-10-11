
/*
 * Copyright (c) 1996,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)init_csparc.c	1.8	98/02/06 SMI"

/*
 * This file defines the known controller types.  To add a new controller
 * type, simply add a new line to the array and define the necessary
 * ops vector in a 'driver' file.
 */
#include "global.h"

extern	struct ctlr_ops md21ops;
extern	struct ctlr_ops scsiops;
extern	struct ctlr_ops ataops;

/*
 * This array defines the supported controller types
 */
struct	ctlr_type ctlr_types[] = {
	{ DKC_MD21,
		"MD21",
		&md21ops,
		CF_SCSI | CF_DEFECTS | CF_OLD_DRIVER },

	{ DKC_SCSI_CCS,
		"SCSI",
		&scsiops,
		CF_SCSI | CF_EMBEDDED | CF_OLD_DRIVER },
	{ DKC_DIRECT,
		"ata",
		&ataops,
		CF_NOFORMAT | CF_NOWLIST },

	{ DKC_PCMCIA_ATA,
		"pcmcia",
		&ataops,
		CF_NOFORMAT | CF_WLIST },
};

/*
 * This variable is used to count the entries in the array so its
 * size is not hard-wired anywhere.
 */
int	nctypes = sizeof (ctlr_types) / sizeof (struct ctlr_type);
