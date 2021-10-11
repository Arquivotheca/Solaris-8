/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ifndef	__VOLUTIL_H
#define	__VOLUTIL_H

#pragma ident	"@(#)volutil.h	1.5	94/09/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern char	*media_findname(char *);
extern char	*media_oldaliases(char *);
extern void	media_printaliases(void);
extern int	volmgt_running(void);
extern int	volmgt_ownspath(char *);
extern int	dev_mounted(char *);
extern int	dev_unmount(char *);

#ifdef	__cplusplus
}
#endif

#endif	/* __VOLUTIL_H */
