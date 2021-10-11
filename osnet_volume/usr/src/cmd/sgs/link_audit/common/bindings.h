/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)bindings.h	1.2	98/01/06 SMI"

#include	<sys/lwp.h>

#define	BINDVERS	1
#define	BINDCURVERS	BINDVERS

#define	DEFFILE		"/tmp/bindings.data"
#define	FILEENV		"BT_BUFFER"
#define	BLKSIZE		0x4000
#define	STRBLKSIZE	0x1000
#define	DEFBKTS		3571			/* nice big prime number */

#define	MASK		(~(unsigned long)0<<28)

typedef struct _bind_entry {
	unsigned int	be_sym_name;
	unsigned int	be_lib_name;
	unsigned int	be_count;
	unsigned int	be_next;
} binding_entry;

typedef struct {
	unsigned int	bb_head;	/* first entry in bucket */
	lwp_mutex_t	bb_lock;	/* bucket chain lock */
} binding_bucket;

typedef struct {
	unsigned int	bh_vers;
	lwp_mutex_t	bh_lock;
	unsigned int	bh_size;
	unsigned int	bh_end;
	unsigned int	bh_bktcnt;
	unsigned int	bh_strcur;		/* current strbuff ptr */
	unsigned int	bh_strend;		/* end of current strbuf */
	lwp_mutex_t	bh_strlock;		/* mutex to protect strings */
	binding_bucket	bh_bkts[DEFBKTS];
} bindhead;
