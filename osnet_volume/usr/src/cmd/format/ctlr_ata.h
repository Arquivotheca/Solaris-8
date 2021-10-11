
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_CTLR_ATA_H
#define	_CTLR_ATA_H

#pragma ident	"@(#)ctlr_ata.h	1.5	98/06/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/buf.h>


/*
 * Rounded parameter, as returned in Extended Sense information
 */
#define	ROUNDED_PARAMETER	0x37


/*
 * Convert a three-byte triplet into an int
 */
#define	TRIPLET(u, m, l)	((int)((((u))&0xff<<16) + \
				(((m)&0xff)<<8) + (l&0xff)))
daddr_t	altsec_offset;		/* Alternate sector offset */

#ifdef	__STDC__
/*
 *	Local prototypes for ANSI C compilers
 */
int	ata_rdwr(int, int, daddr_t, int, caddr_t, int, int *);
int	ata_ck_format(void);
int	ata_ex_man(struct defect_list *);
int	ata_ex_cur(struct defect_list *);
int	ata_ex_grown(struct defect_list *);
int	ata_read_defect_data(struct defect_list *, int);
int	ata_repair(int, int);
int	apply_chg_list(int, int, u_char *, u_char *, struct chg_list *);
int	ata_wr_cur(struct defect_list *);

#else

int	ata_rdwr();
int	ata_ck_format();
int	ata_ex_man();
int	ata_ex_cur();
int	ata_ex_grown();
int	ata_read_defect_data();
int	ata_repair();
int	apply_chg_list();
int	ata_wr_cur();

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _CTLR_ATA_H */
