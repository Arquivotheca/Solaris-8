/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *  probe.h -- public definitions for probe module
 */

#ifndef _PROBE_H
#define	_PROBE_H

#ident "@(#)probe.h   1.14   98/04/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	DPROBE_PRINT	0x1	/* output info to users */
#define	DPROBE_ALL	0x2	/* probe using all befs */
#define	DPROBE_LIST	0x4	/* probe using list argument */
#define	DPROBE_ALWAYS	0x8	/* probe using "probe-always" befs */

int device_probe(char **drivers, int flags);
Resource *primary_probe(Board *bp);
void ask_all_probe(void);
void do_all_probe(void);
int do_selected_probe(void);
void deassign_prog_probe();
void assign_prog_probe();

#ifdef	__cplusplus
}
#endif

#endif	/* _PROBE_H */
