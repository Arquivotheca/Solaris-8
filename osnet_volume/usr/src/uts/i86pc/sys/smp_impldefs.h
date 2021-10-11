/*
 * Copyright (c) 1992-1993, 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SMP_IMPLDEFS_H
#define	_SYS_SMP_IMPLDEFS_H

#pragma ident	"@(#)smp_impldefs.h	1.15	99/06/05 SMI"

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/cpuvar.h>
#include <sys/pic.h>
#include <sys/xc_levels.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	WARM_RESET_VECTOR	0x467	/* the ROM/BIOS vector for 	*/
					/* starting up secondary cpu's	*/

/*
 *	External Reference Functions
 */
extern void (*psminitf)();	/* psm init entry point			*/
extern void (*picinitf)();	/* pic init entry point			*/
extern void (*clkinitf)();	/* clock init entry point		*/
extern int (*ap_mlsetup)(); 	/* completes init of starting cpu	*/
extern void (*send_dirintf)();	/* send interprocessor intr		*/
extern void (*cpu_startf)();	/* start running a given processor	*/
extern hrtime_t (*gethrtimef)(); /* get high resolution timer value	*/
extern void (*psm_shutdownf)(int, int);	/* machine dependent shutdown	*/
extern void (*psm_notifyf)(int); /* PSMI module notification		*/
extern void (*psm_set_idle_cpuf)(processorid_t); /* cpu changed to idle */
extern void (*psm_unset_idle_cpuf)(processorid_t); /* cpu out of idle 	*/
extern int (*psm_disable_intr)(processorid_t); /* disable intr to cpu	*/
extern void (*psm_enable_intr)(processorid_t); /* enable intr to cpu	*/
extern int (*psm_get_clockirq)(int); /* get clock vector		*/
extern int (*psm_get_ipivect)(int, int); /* get interprocessor intr vec */

extern int (*slvltovect)(int);	/* ipl interrupt priority level		*/
extern int (*setlvl)(int, int *); /* set intr pri represented by vect	*/
extern void (*setlvlx)(int, int); /* set intr pri to specified level	*/
extern void (*setspl)(int);	/* mask intr below or equal given ipl	*/
extern int (*addspl)(int, int, int, int); /* add intr mask of vector 	*/
extern int (*delspl)(int, int, int, int); /* delete intr mask of vector */
extern void (*setsoftint)(int);	/* trigger a software intr		*/

extern uint_t xc_serv(caddr_t);	/* cross call service routine		*/
extern void set_pending();	/* set software interrupt pending	*/
extern void clksetup(void);	/* timer initialization 		*/
extern void microfind(void);

/* map physical address							*/
extern caddr_t psm_map_phys(paddr_t, long, ulong_t);
/* unmap the physical address given in psm_map_phys() from the addr	*/
extern void psm_unmap_phys(caddr_t, long);
extern void psm_modloadonly(void);
extern void psm_install(void);
extern void psm_modload(void);

/*
 *	External Reference Data
 */
extern struct av_head autovect[]; /* array of auto intr vectors		*/
extern int clock_vector;	/* clock interrupt vector		*/
extern paddr_t rm_platter_pa;	/* phy addr realmode startup storage	*/
extern caddr_t rm_platter_va;	/* virt addr realmode startup storage	*/
extern int mp_cpus;		/* bit map of possible cpus found	*/

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SMP_IMPLDEFS_H */
