
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef	_PROMPTS_H
#define	_PROMPTS_H

#pragma ident	"@(#)prompts.h	1.5	97/05/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


int	get_ncyl(void);
int	get_acyl(int n_cyls);
int	get_pcyl(int n_cyls, int p_cyls);
int	get_nhead(void);
int	get_phead(int n_heads, u_long *option);
int	get_nsect(void);
int	get_psect(u_long *option);
int	get_bpt(int n_sects, u_long *option);
int	get_rpm(void);
int	get_fmt_time(u_long *option);
int	get_cyl_skew(u_long *option);
int	get_trk_skew(u_long *option);
int	get_trks_zone(u_long *option);
int	get_atrks(u_long *option);
int	get_asect(u_long *option);
int	get_cache(u_long *option);
int	get_threshold(u_long *option);
int	get_min_prefetch(u_long *option);
int	get_max_prefetch(int min_prefetch, u_long *option);
int	get_bps(void);
char	*get_asciilabel(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _PROMPTS_H */
