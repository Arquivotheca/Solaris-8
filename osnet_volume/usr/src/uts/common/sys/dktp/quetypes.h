/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_DKTP_QUETYPES_H
#define	_SYS_DKTP_QUETYPES_H

#pragma ident	"@(#)quetypes.h	1.4	94/09/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct que_data {
	kmutex_t	q_mutex;
	struct diskhd	q_tab;
};

#define	q_cnt		q_tab.b_bcount

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_QUETYPES_H */
