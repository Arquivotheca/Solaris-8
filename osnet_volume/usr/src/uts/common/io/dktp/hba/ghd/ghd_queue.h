/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _GHD_QUEUE_H
#define	_GHD_QUEUE_H

#pragma	ident	"@(#)ghd_queue.h	1.5	99/04/09 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 *  A list of singly linked elements
 */

typedef struct L1el {
	struct L1el	*le_nextp;
	void		*le_datap;
} L1el_t;

#define	L1EL_INIT(lep)	((lep)->le_nextp = NULL, (lep)->le_datap = 0)

typedef struct L1_head {
	L1el_t	*l1_headp;
	L1el_t	*l1_tailp;
} L1_t;

#define	L1HEADER_INIT(lp) (((lp)->l1_headp = NULL), ((lp)->l1_tailp = NULL))
#define	L1_EMPTY(lp)	((lp)->l1_headp == NULL)

void	 L1_add(L1_t *lp, L1el_t *lep, void *datap);
void	 L1_delete(L1_t *lp, L1el_t *lep);
void	*L1_remove(L1_t *lp);


/*
 * A list of doubly linked elements
 */

typedef struct L2el {
	struct	L2el	*l2_nextp;
	struct	L2el	*l2_prevp;
	void		*l2_private;
} L2el_t;

#define	L2_INIT(headp)	\
	(((headp)->l2_nextp = (headp)), ((headp)->l2_prevp = (headp)))

#define	L2_EMPTY(headp) ((headp)->l2_nextp == (headp))

void	L2_add(L2el_t *headp, L2el_t *elementp, void *private);
void	L2_delete(L2el_t *elementp);
void	L2_add_head(L2el_t *headp, L2el_t *elementp, void *private);
void	*L2_remove_head(L2el_t *headp);
void	*L2_next(L2el_t *elementp);


#ifdef	__cplusplus
}
#endif
#endif  /* _GHD_QUEUE_H */
