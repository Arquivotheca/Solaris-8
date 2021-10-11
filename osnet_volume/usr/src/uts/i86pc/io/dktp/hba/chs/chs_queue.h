/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef _CHS_CHS_QUEUE_H
#define	_CHS_CHS_QUEUE_H

#pragma	ident	"@(#)chs_queue.h	1.3	99/03/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *  command list queue
 */

#define	QEMPTY(QP)	((QP)->qh_headp == NULL)
#define	QUEUE_INIT(QP)	((QP)->qh_headp = NULL, (QP)->qh_tailpp = NULL)

typedef struct queue_element {
	struct queue_element	*qe_linkp;
	void			*qe_datap;
} Qel_t;

typedef struct queue_head {
	Qel_t	 *qh_headp;
	Qel_t	**qh_tailpp;
#ifdef CHS_DEBUG
	ulong_t	  qh_add;
	ulong_t	  qh_rm;
#endif
} Que_t;

void	 QueueAdd(Que_t *qp, Qel_t *qelp, void *datap);
void	*QueueRemove(Que_t *qp);

#ifdef	__cplusplus
}
#endif

#endif  /* _CHS_CHS_QUEUE_H */
