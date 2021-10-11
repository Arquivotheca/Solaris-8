/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *  Bus Resource Management routines:
 *
 *    These routines can be used to keep track of bus resources assigned
 *    to various devices during the auto-configuartion process.  There are
 *    five types of resources:
 *
 *	    Slot  ...  EISA device slots
 *	    Port  ...  I/O port ranges
 *	    Mem   ...  Memory buffer ranges
 *	    Irq   ...  Interrupt request lines
 *	    Dma   ...  DMA channels
 *
 *    If a given device uses a particular resource, there will be a Resource
 *    record of that type associated with one of the device functions (except
 *    for slot resources, which are indicated in the "Board" record).  The
 *    following routines may be used to search search a board list for a
 *    resource conflict:
 *
 *    Board *Query_resmgmt(Board *bp, unsigned t, ulong a, ulong l)
 *
 *	  Returns a pointer to the Board that uses the indicated resource,
 *	  or NULL if there is no such board. The resource must be uniquely
 *	  identified by "t"ype, "a"ddress, and "l"ength.
 */

#ifndef _RESMGMT_H
#define	_RESMGMT_H

#ident "@(#)resmgmt.h   1.14   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

Board *Query_resmgmt(Board *bp, u_int t, u_long a, u_long l);

#define	RESF_Slot 0			/* Not a true resource type!	    */

/*
 *  Normally, one does not use the invoke the above routines directly.  The
 *  following macro versions are used instead:
 */
#define	Query_Port(a, l) Query_resmgmt(Head_board, RESF_Port, a, l)
#define	Query_Mem(a, l)  Query_resmgmt(Head_board, RESF_Mem,  a, l)
#define	Query_Irq(n)	 Query_resmgmt(Head_board, RESF_Irq,  n, 1)
#define	Query_Dma(n)	 Query_resmgmt(Head_board, RESF_Dma,  n, 1)

/*
 * Check for any conflicts betwwen target Board bp, and
 * the rest of the resources on Head_board.
 */
Resource *board_conflict_resmgmt(Board *bp, short test_weak, Board **cbp);
/*
 * Check if any boards gave up weak resources
 */
void check_weak();

#ifdef	__cplusplus
}
#endif

#endif /* _RESMGMT_H */
