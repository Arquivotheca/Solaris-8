/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)dpt_eisa.c	1.2	99/03/03 SMI"

#include <sys/types.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/kmem.h>

#include <sys/eisarom.h>
#include <sys/nvm.h>

#include "dpt_eisa.h"

int	eisa_nvm(char *data, KEY_MASK key_mask, ...);

/*
 * check if an EISA NVRAM has the right type of board
 */
int
dpt_eisa_probe_nvm(ushort slotadr, ulong board_id, ulong rev_mask)
{
	struct	{
		short	slotnum;
		NVM_SLOTINFO	slot;
		NVM_FUNCINFO	func;
	} buff;
	int		slotnum = (slotadr >> 12);
	ulong		nvm_bid;
	KEY_MASK	key_mask = {0};

	key_mask.slot = TRUE;
	key_mask.function = TRUE;

	/* get the slot record and the first function record */
	if (!eisa_nvm((char *)&buff, key_mask, slotnum, 0)) {
		/* shouldn't happen, no functions */
		return (FALSE);
	}
	if (slotnum != buff.slotnum) {
		/* shouldn't happen, eisa nvram mismatch */
		return (FALSE);
	}
	nvm_bid = *((ulong *)(&buff.slot.boardid[0]));
	return ((nvm_bid & rev_mask) == (board_id & rev_mask));
}


int
dpt_eisa_check_id(ushort ioaddr, pid_spec_t *idp, int cnt)
{
	int	idx;

	for (idx = 0; idx < cnt; idx++, idp++) {
		if (dpt_eisa_probe_nvm(ioaddr, idp->id, idp->mask)) {
			return (TRUE);
		}
	}
	return (FALSE);
}



int
dpt_eisa_probe(dev_info_t	*dip,
		ushort		 ioaddr,
		pid_spec_t	*default_pid,
		int		 cnt)
{
	pid_spec_t	*prod_idp;	/* ptr to the product id property */
	int		 prod_idlen;	/* length of the array */
	int		 rc;

	rc = ddi_getlongprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "product_id", (caddr_t)&prod_idp, &prod_idlen);

	if (rc == DDI_PROP_SUCCESS) {
		rc = dpt_eisa_check_id(ioaddr, prod_idp,
		    prod_idlen / sizeof (pid_spec_t));
		kmem_free((caddr_t)prod_idp, prod_idlen);
	} else {
		rc = dpt_eisa_check_id(ioaddr, default_pid, cnt);
	}
	return (rc);
}
