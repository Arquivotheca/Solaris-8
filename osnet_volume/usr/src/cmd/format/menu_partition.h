
/*
 * Copyright (c) 1991-1994 by Sun Microsystems, Inc.
 */

#ifndef	_MENU_PARTITION_H
#define	_MENU_PARTITION_H

#pragma ident	"@(#)menu_partition.h	1.9	94/06/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 *	Prototypes for ANSI C compilers
 */
int	p_apart(void);
int	p_bpart(void);
int	p_cpart(void);
int	p_dpart(void);
int	p_epart(void);
int	p_fpart(void);
int	p_gpart(void);
int	p_hpart(void);

#if defined(i386)
int	p_jpart(void);
#endif			/* defined(i386) */

int	p_select(void);
int	p_modify(void);
int	p_name(void);
int	p_print(void);

void	print_map(struct partition_info *map);
void	print_partition(struct partition_info *pinfo, int partnum,
		int want_header);

int	chk_volname(struct disk_info *);
void	print_volname(struct disk_info *);
void	nspaces(int);
int	ndigits(int);

#ifdef	__cplusplus
}
#endif

#endif	/* _MENU_PARTITION_H */
