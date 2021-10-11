
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef	_CTLR_MD21_H
#define	_CTLR_MD21_H

#pragma ident	"@(#)ctlr_md21.h	1.11	98/06/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Hardwired mode sense/mode select parameters
 */
#define	READ_RETRIES		16		/* default read retries */
#define	BUS_INACTIVITY_LIMIT	40		/* bus inactivity limit */


/*
 *	Prototypes for ANSI C compilers
 */
int	md_rdwr(int, int, daddr_t, int, caddr_t, int, int *);
int	md_ck_format(void);
int	md_format(int, int, struct defect_list *);
int	md_ex_man(struct defect_list *);
int	md_ex_cur(struct defect_list *);
int	md_repair(int, int);
void	convert_old_list_to_new(struct defect_list *,
		struct scsi_format_params *);

#ifdef	__cplusplus
}
#endif

#endif	/* _CTLR_MD21_H */
