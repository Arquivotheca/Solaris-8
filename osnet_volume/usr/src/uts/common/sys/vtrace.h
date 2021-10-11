/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989,1995,1997-1999  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_VTRACE_H
#define	_SYS_VTRACE_H

#pragma ident	"@(#)vtrace.h	2.95	99/07/26 SMI"

#ifndef	_ASM
#include <sys/types.h>
#include <sys/time.h>
#ifdef	_KERNEL
#include <sys/cpuvar.h>
#endif	/* _KERNEL */
#endif	/* _ASM */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file defines the vtrace tracing mechanism.  The trace record format
 * described here is used for both kernel and user level tracing.
 *
 * Each trace record consists of a header word followed by up to 63
 * words of data.  The number of data words is constant for each type
 * of trace record, i.e., for each event (see definitions below).
 * The header word has the following format:
 *
 * -------------------------------------
 * |  FAC   |  TAG   |   TIME DELTA    |
 * -------------------------------------
 *   31..24   23..16	   15..0
 *
 * That is, header = (FAC << 24) | (TAG << 16) | TIME,
 *
 * where:
 *
 * FAC  = 8-bit facility tag: VM, STREAMS, UFS, ...
 * TAG  = 8-bit tag within that facility
 * TIME = 16-bit hi-res time delta since the last trace record
 *
 * The (facility, tag) pair defines the event:
 *
 * EVENT = (FAC << 8) | TAG
 *
 * Therefore, the header can also be viewed as
 *
 * -------------------------------------
 * |	  EVENT	    |	 TIME DELTA    |
 * -------------------------------------
 *	  31..16	   15..0
 *
 * That is, header = (EVENT << 16) | TIME.
 *
 * Key points:
 *
 * 1. As shown above, each event is identified by a 16-bit ID, which can
 *    also be viewed as a (facility, tag) byte pair.  Facilities provide a
 *    logical partitioning of the event name space, e.g. UFS, VM, SYSCALL, etc.
 *    Tracing can be enabled/disabled by facility or by individual event.
 *
 * 2. vtrace keeps track of the writing thread ID.  When the writing thread
 *    changes, vtrace automatically issues a special thread ID record.
 *
 * 3. We write out the CPU number only once, since each CPU has a
 *    separate trace buffer.  For MP systems, we use a post-processing
 *    program (trmerge) to merge and time-sort the trace files for
 *    each CPU into a single, time-ordered file.  trmerge generates
 *    synthetic trace records to indicate changes in the writing CPU.
 *
 * 4. The facility space is partitioned into [0..127] for kernel trace
 *    events, and [128..255] for user-level trace events, so you can
 *    merge kernel and user trace files without namespace collisions.
 *
 * 5. The first time an event is encountered, its trace record is
 *    preceded by a TR_LABEL record.  (The version record is the lone
 *    exception, since it must be the first record in the file - see below.)
 *    The label defines the number and type of data fields for that event.
 *    For example, consider the trace point:
 *
 *		TRACE_3(TR_FAC_FOO, TR_BAR,
 *			"foobar:year %d message %s gross %d",
 *			1992, "hello world", 144);
 *
 *    The TR_LABEL record for this event would contain:
 *
 *		- facility (TR_FAC_FOO)
 *		- tag (TR_BAR)
 *		- size (4 words: 1 header word plus 3 data words)
 *		- name ("foobar")
 *		- format ("year %d message %s gross %d")
 *		- a bitmap indicating which data fields are numbers (1 and 3)
 *		  and which are strings (2).
 *
 *    Each TR_LABEL record is followed by a TR_DATA record (see below)
 *    containing the name and format strings for the event, stored as
 *    name\0format\0.
 *
 *    WARNING: Each event must have a *unique* label.  Therefore, if
 *    you put in a pair of trace points in routine foo_lookup() like:
 *
 *	TRACE_0(TR_FAC_FOO, TR_FOO_LOOKUP_END, "foo_lookup_end:error");
 *
 *    and
 *
 *	TRACE_0(TR_FAC_FOO, TR_FOO_LOOKUP_END, "foo_lookup_end:success");
 *
 *    this will *not* have the desired effect.  Whichever trace point is
 *    encountered first will determine the label for all subsequent
 *    (TR_FAC_FOO, TR_FOO_LOOKUP_END) trace points.  Thus, if the first
 *    trace point encountered in foo_lookup() was the error case, then
 *    every foo_lookup() call would appear in the trace file as though
 *    it exited with an error.
 *
 *    There are two acceptable ways to handle cases like this:
 *
 *	(1) Use different tags for the different cases:
 *
 *	TRACE_0(TR_FAC_FOO, TR_FOO_LOOKUP_ERROR, "foo_lookup_end:error");
 *	TRACE_0(TR_FAC_FOO, TR_FOO_LOOKUP_SUCCESS, "foo_lookup_end:success");
 *
 *	(2) Use a data field to contain the extra information:
 *
 *	TRACE_1(TR_FAC_FOO, TR_FOO_LOOKUP_END,
 *		"foo_lookup_end:exit code %d", exit_code);
 *
 * 6. In order to support string names and other variable length data
 *    items, we define several TR_DATA records.  These records come in
 *    4, 8, 16, 32, and 64-word sizes, and hold 3, 7, 15, 31, or 63 words
 *    of data, respectively, in addition to the usual header word.
 *    Data records are appended to any trace records that need to hold
 *    strings or other variable-size data.  All strings must be null-terminated;
 *    thus, the maximum string length is 63 * 4 - 1 = 251 characters.
 *    Longer strings are automatically truncated.
 *
 * 7. When a trace record contains string data, the actual string is stored
 *    in a subsequent TR_DATA record.  The data field corresponding to the
 *    string contains the offset into the trace file where the string begins.
 *    For example, if our example trace point
 *
 *		TRACE_3(TR_FAC_FOO, TR_BAR,
 *			"foobar:year %d message %s gross %d",
 *			1992, "hello world", 144);
 *
 *    began at offset 3000 in the trace file, the contents of the trace file
 *    would look like this:
 *
 *	offset	word
 *	3000	header word = FTT2HEAD (TR_FAC_FOO, TR_BAR, time_delta)
 *	3004	1992
 *	3008	3020
 *	3012	144
 *	3016	header word = FTT2HEAD (TR_FAC_TRACE, TR_DATA_4, 0)
 *	3020	"hell"
 *	3024	"o wo"
 *	3028	"rld\0"
 *
 * 8. Trace files have the following format after trmerge post-processing:
 *
 *	- header section:
 *
 *		- version record
 *		- label records for all events
 *		- start_hrtime record
 *		- abs_time record
 *		- pagesize record
 *		- num_cpus record
 *		- cpu record
 *		- title record
 *		- thread ID of tracing process
 *		- padding to the end of the page
 *
 *	- main section; each page looks like:
 *
 *		- total time record
 *		- kthread id record (or uthread id record)
 *		- writing CPU record
 *		- trace data
 *		- padding to the end of the page, if the next trace record
 *		  doesn't fit
 *
 * 9. GUIDELINES FOR ADDING TRACE POINTS:
 *
 *	If you are adding a new facility:
 *
 *	- THE MOST IMPORTANT THING: remember that the kernel only owns
 *	  facilities 2 through 127.  0 and 1 are reserved, and 128 through
 *	  255 belong to user-level tracing.
 *
 *	- Facility names should all begin with TR_FAC.
 *
 *	- Try to pick a meaningful, descriptive name.  If the trace points
 *	  only occur in one module, the module's name is probably a good
 *	  choice, e.g. TR_FAC_AUDIO.  In particular, each filesystem should
 *	  have its own facility.  On the other hand, if you're putting
 *	  trace points into bwtwo, consider defining a TR_FAC_FRAMEBUFFER
 *	  (with tags for bwtwo, cgsix, etc.) instead of defining TR_FAC_BWTWO,
 *	  so we don't exhaust the facility namespace.
 *
 *	If you are adding new tags:
 *
 *	- Tag names should begin with TR.
 *
 *	- Again, try to pick a meaningful, descriptive name.  In particular,
 *	  if you are defining trace points for the start and end of routine
 *	  foo, use TR_FOO_START and TR_FOO_END.  If there are multiple entry/
 *	  exit points, try to follow the example of the TR_PAGE_CREATE tags
 *	  below.
 *
 *	Adding the actual trace points:
 *
 *	- A trace point has the form:
 *
 *	TRACE_n(fac, tag, "name:format" , data_1, ... ,data_n);
 *
 *	where:
 *		n	= number of data words
 *		fac	= facility
 *		tag	= tag
 *		name	= name of the event
 *		format	= format string (printf style) for printing the data
 *		data_*	= any data that fits in a long/ulong_t, or a pointer
 *			= to a string
 *
 *	- Examples:
 *
 *	TRACE_3(TR_FAC_UFS, TR_UFS_READ_START,
 *		"ufs_read_start:vp %x uiop %x ioflag %x", vp, uiop, ioflag);
 *
 *	TRACE_2(TR_FAC_UFS, TR_UFS_LOOKUP_START,
 *		"ufs_lookup_start:dvp %x name %s", dvp, nm);
 *
 *	TRACE_0(TR_FAC_TRAP, TR_TRAP_END, "trap_end");
 *
 *	- The name:format string should consist of the event name, a colon,
 *	  and the format string for printing the data.  (If there is no data,
 *	  you can omit the colon and format string, as in the third example.)
 *
 *	- In addition to the usual %d, %x, %s, etc., format strings may also
 *	  contain %K, which denotes a kernel address, or %T, which denotes
 *	  a kernel thread ID.  trmerge converts %K kernel addresses into
 *	  symbolic names, and %T kthread IDs into thread names.
 *	  See bcopy() for sample usage of %K; see disp() for sample usage
 *	  of %T.
 */

/*
 * Facility definitions
 */

#define	TR_FAC_TRACE		0	/* administrative trace records */
#define	TR_FAC_TEST		1	/* kernel trace mechanism tests */
#define	TR_FAC_TRAP		2	/* traps */
#define	TR_FAC_INTR		3	/* interrupts */
#define	TR_FAC_SYSCALL		4	/* system calls */
#define	TR_FAC_DISP		5	/* dispatcher */
#define	TR_FAC_VM		6	/* VM system */
#define	TR_FAC_PROC		7	/* process subsystem */
#define	TR_FAC_LOCK		8	/* locks: mutex, rw, cv */
#define	TR_FAC_BCOPY		9	/* bcopy and friends */
#define	TR_FAC_KMEM		10	/* kmem_alloc(), etc. */
#define	TR_FAC_STREAMS_FR	11	/* STREAMS framework */
#define	TR_FAC_STREAMS_MOD	12	/* misc STREAMS module or driver */
#define	TR_FAC_SOCKMOD		13	/* streams socket module */
#define	TR_FAC_TCP		14	/* tcp protocol module */
#define	TR_FAC_UDP		15	/* udp protocol module */
#define	TR_FAC_IP		16	/* ip protocol module */
#define	TR_FAC_ARP		17	/* arp protocol module */
#define	TR_FAC_LE		18	/* lance ethernet driver */
#define	TR_FAC_IPI		19	/* IPI driver */
#define	TR_FAC_SCSI		21	/* SCSI */
#define	TR_FAC_LWP		22	/* lwps */
#define	TR_FAC_SYS_LWP		23	/* lwp system calls */
#define	TR_FAC_CALLOUT		24	/* callout table */
#define	TR_FAC_SPECFS		29	/* specfs fileystem */
#define	TR_FAC_SWAPFS		30	/* specfs fileystem */
#define	TR_FAC_TMPFS		31	/* specfs fileystem */
#define	TR_FAC_UFS		32	/* UFS */
#define	TR_FAC_NFS		33	/* NFS */
#define	TR_FAC_DDI		35	/* DDI */
#define	TR_FAC_KRPC		36	/* Kernel RPC */
#define	TR_FAC_SCHED		37	/* swapper */
#define	TR_FAC_SCSI_RES		38	/* SCSI_RESOURCE */
#define	TR_FAC_SCSI_ISP		39	/* ISP HBA Driver SCSI */
#define	TR_FAC_IA		40	/* IA scheduling class */
#define	TR_FAC_S5		41	/* S5 */
#define	TR_FAC_QE		42	/* QED Ethernet driver */
#define	TR_FAC_BE		43	/* Fast Ethernet driver */
#define	TR_FAC_FIFO		44	/* Fifos */
#define	TR_FAC_RLOGINP		45	/* rlmod protocol module */
#define	TR_FAC_AE		46	/* PCnet (Am79C970) ethernet driver */
#define	TR_FAC_PHYSIO		47	/* physio */
#define	TR_FAC_META		48	/* meta disk */
#define	TR_FAC_SCSI_FAS		49	/* fas scsi HBA driver */
#define	TR_FAC_SOCKFS		50	/* socket fileystem */
#define	TR_FAC_DEVMAP		51	/* devmap */
#define	TR_FAC_DADA		52	/* target driver for ide */

/*
 * TR_FAC_TRACE tags -- for internal use.  All times are in microseconds.
 */

#define	TR_END			0	/* end of trace file */
#define	TR_VERSION		1	/* trace record format */
#define	TR_TITLE		2	/* title of trace file */
#define	TR_PAD 			3	/* pad record, to fill out a page */
#define	TR_LABEL		4	/* trace record label */
#define	TR_PAGESIZE		5	/* system page size */
#define	TR_NUM_CPUS		6	/* number of CPUs on the system */
#define	TR_CPU			7	/* which cpu is writing */
#define	TR_DATA_4		8	/* generic data, header + 3 words */
#define	TR_DATA_8		9	/* generic data, header + 7 words */
#define	TR_DATA_16		10	/* generic data, header + 15 words */
#define	TR_DATA_32		11	/* generic data, header + 31 words */
#define	TR_DATA_64		12	/* generic data, header + 63 words */
#define	TR_ABS_TIME		13	/* absolute time since epoch */
#define	TR_START_TIME		14	/* 64-bit time at start of trace */
#define	TR_ELAPSED_TIME		15	/* 32-bit time since previous trace */
#define	TR_TOTAL_TIME		16	/* 64-bit time since start of trace */
#define	TR_KTHREAD_ID		17 	/* kernel thread id */
#define	TR_UTHREAD_ID		18 	/* user thread id */
#define	TR_CLOCK_FREQUENCY	19	/* clock rate for vtrace timestamps */

/*
 * The rest of the TR_FAC_TRACE tags appear only in raw trace files:
 */

#define	TR_RAW_KTHREAD_ID	64 	/* raw (trace-time) kernel thread id */
#define	TR_RAW_UTHREAD_ID	65 	/* raw (trace-time) user thread id */
#define	TR_KTHREAD_LABEL	66	/* kernel thread label */
#define	TR_UTHREAD_LABEL	67	/* user thread label */
#define	TR_PROCESS_NAME		68 	/* ps-style process name */
#define	TR_PROCESS_FORK		69 	/* generated when a process forks */

/*
 * TR_FAC_TEST tags -- diagnostics for the trace mechanism
 */

#define	TR_SPEED_0		0
#define	TR_SPEED_1		1
#define	TR_SPEED_1_STRING	2
#define	TR_SPEED_2		3
#define	TR_SPEED_2_STRING	4
#define	TR_SPEED_3		5
#define	TR_SPEED_3_STRING	6
#define	TR_SPEED_4		7
#define	TR_SPEED_4_STRING	8
#define	TR_SPEED_5		9
#define	TR_SPEED_5_STRING	10
#define	TR_TRACE_FLUSH_START	11
#define	TR_TRACE_FLUSH_END	12
#define	TR_TRACE_VN_WRITE_START	13
#define	TR_TRACE_VN_WRITE_END	14

/*
 * TR_FAC_TRAP tags
 */

#define	TR_TRAP_START			0
#define	TR_TRAP_END			1
#define	TR_KERNEL_WINDOW_OVERFLOW	2
#define	TR_KERNEL_WINDOW_UNDERFLOW	3
#define	TR_USER_WINDOW_OVERFLOW		4
#define	TR_USER_WINDOW_UNDERFLOW	5
#define	TR_C_TRAP_HANDLER_ENTER		6
#define	TR_C_TRAP_HANDLER_EXIT		7

/*
 * TR_FAC_INTR tags
 */

#define	TR_INTR_START		0
#define	TR_INTR_END		1
#define	TR_INTR_EXIT		2
#define	TR_INTR_PASSIVATE	3

/*
 * TR_FAC_SYSCALL tags
 */

#define	TR_SYSCALL_START	0
#define	TR_SYSCALL_END		1

/*
 * TR_FAC_DISP tags
 */

#define	TR_DISP_START		0
#define	TR_DISP_END		1
#define	TR_SWTCH_START		2
#define	TR_SWTCH_END		3
#define	TR_PREEMPT_START	4
#define	TR_PREEMPT_END		5
#define	TR_RESUME_START		6
#define	TR_RESUME_END		7
#define	TR_FRONTQ		8
#define	TR_BACKQ		9
#define	TR_CPU_RESCHED		10
#define	TR_SLEEP		11
#define	TR_TRAPRET		12
#define	TR_TICK			13
#define	TR_UPDATE		14
#define	TR_CPU_CHOOSE		15
#define	TR_CPU_SURRENDER	16
#define	TR_PREEMPT		17


/*
 * TR_FAC_VM tags
 */

#define	TR_PAGE_INIT		0
#define	TR_PAGE_WS_IN		1
#define	TR_PAGE_WS_OUT		2
#define	TR_PAGE_WS_FREE		3
#define	TR_PAGE_WS_RECLAIM	4
#define	TR_PAGEOUT_START	5
#define	TR_PAGEOUT_END		6
#define	TR_PAGEOUT_HAND_WRAP	7
#define	TR_PAGEOUT_MAXPGIO	8
#define	TR_PAGEOUT_ISREF	9
#define	TR_PAGEOUT_FREE		10
#define	TR_PAGEOUT_CV_SIGNAL	11
#define	TR_HAT_SETMOD		12
#define	TR_HAT_SETREF		13
#define	TR_HAT_SETREFMOD	14
#define	TR_HAT_CLRMOD		15
#define	TR_HAT_CLRREF		16
#define	TR_HAT_CLRREFMOD	17
#define	TR_HME_ADD		18
#define	TR_HME_SUB		19
#define	TR_SEGMAP_FAULT		20
#define	TR_SEGMAP_GETMAP	21
#define	TR_SEGMAP_RELMAP	22
#define	TR_SEGMAP_PAGECREATE	23
#define	TR_SEGMAP_GETPAGE	24
#define	TR_SEGVN_FAULT		25
#define	TR_SEGVN_GETPAGE	26
#define	TR_ANON_GETPAGE		27
#define	TR_ANON_PRIVATE		28
#define	TR_SEGKMEM_ALLOC	29
#define	TR_SWAP_ALLOC		30
#define	TR_PVN_READ_KLUSTER	31
#define	TR_PVN_GETDIRTY		32
#define	TR_PAGE_CREATE_START	33
#define	TR_PAGE_CREATE_TOOBIG	34
#define	TR_PAGE_CREATE_NOMEM	35
#define	TR_PAGE_CREATE_SUCCESS	36
#define	TR_PAGE_CREATE_SLEEP_START	37
#define	TR_PAGE_CREATE_SLEEP_END	38
#define	TR_PAGE_CREATE_ALLOC	39
#define	TR_PAGE_FREE_FREE	40
#define	TR_PAGE_FREE_CACHE_HEAD	41
#define	TR_PAGE_FREE_CACHE_TAIL	42
#define	TR_PAGE_UNFREE_FREE	43
#define	TR_PAGE_UNFREE_CACHE	44
#define	TR_PAGE_DESTROY		45
#define	TR_PAGE_HASHIN		46
#define	TR_PAGE_HASHOUT		47
#define	TR_ANON_PROC		48
#define	TR_ANON_SHM		49
#define	TR_ANON_TMPFS		50
#define	TR_ANON_SWAP		51
#define	TR_ANON_EXEC		52
#define	TR_ANON_SEGKP		53
#define	TR_SAMPLE_REF		54
#define	TR_SAMPLE_MOD		55
#define	TR_SAMPLE_WS_START	56
#define	TR_SAMPLE_WS_END	57
#define	TR_KAS_INFO		58
#define	TR_AS_INFO		59
#define	TR_SEG_INFO		60
#define	TR_PAGE_RENAME		61
#define	TR_SWAP_RENAME		62

/*
 * TR_FAC_PROC tags
 */

#define	TR_PROC_EXEC		0
#define	TR_PROC_EXIT		1
#define	TR_PROC_FORK		2
#define	TR_EXECMAP_PREREAD	3
#define	TR_EXECMAP_NO_PREREAD	4

/*
 * TR_FAC_SCHED tags
 */

#define	TR_SWAPIN		0
#define	TR_SWAPOUT		1
#define	TR_RUNIN		2
#define	TR_RUNOUT		3
#define	TR_CHOOSE_SWAPOUT	4
#define	TR_CHOOSE_SWAPIN	5
#define	TR_SOFTSWAP		6
#define	TR_HARDSWAP		7
#define	TR_DESPERATE		8
#define	TR_HIGH_DEFICIT		9
#define	TR_SWAPIN_VALUES	10
#define	TR_UNLOAD		11
#define	TR_SWAPOUT_LWP		12
#define	TR_SWAPQ_LWP		13
#define	TR_SWAPQ_PROC		14

/*
 * TR_FAC_LOCK tags
 */

#define	TR_RW_ENTER_RD_START	0
#define	TR_RW_ENTER_RD_END	1
#define	TR_RW_ENTER_WR_START	2
#define	TR_RW_ENTER_WR_END	3
#define	TR_RW_EXIT_START	4
#define	TR_RW_EXIT_END		5

/*
 * TR_FAC_BCOPY tags
 */

#define	TR_BCOPY_START		0
#define	TR_KCOPY_START		1
#define	TR_PGCOPY_START		2
#define	TR_COPYOUT_START	3
#define	TR_COPYIN_START		4
#define	TR_COPY_END		5
#define	TR_COPY_FAULT		6
#define	TR_COPYOUT_FAULT	7
#define	TR_COPYIN_FAULT		8
#define	TR_COPYIN_NOERR_START	9
#define	TR_COPYOUT_NOERR_START	10

/*
 * TR_FAC_KMEM tags
 */

#define	TR_KMEM_ALLOC_START		0
#define	TR_KMEM_ALLOC_END		1
#define	TR_KMEM_ZALLOC_START		2
#define	TR_KMEM_ZALLOC_END		3
#define	TR_KMEM_FREE_START		4
#define	TR_KMEM_FREE_END		5
#define	TR_KMEM_CACHE_ALLOC_START	12
#define	TR_KMEM_CACHE_ALLOC_END		13
#define	TR_KMEM_CACHE_FREE_START	14
#define	TR_KMEM_CACHE_FREE_END		15
#define	TR_KMEM_SLAB_CREATE_START	16
#define	TR_KMEM_SLAB_CREATE_END		17
#define	TR_KMEM_SLAB_DESTROY_START	18
#define	TR_KMEM_SLAB_DESTROY_END	19
#define	TR_KMEM_GETPAGES_START		20
#define	TR_KMEM_GETPAGES_END		21
#define	TR_KMEM_FREEPAGES_START		22
#define	TR_KMEM_FREEPAGES_END		23
#define	TR_KMEM_ASYNC_DISPATCH_START	24
#define	TR_KMEM_ASYNC_DISPATCH_END	25
#define	TR_KMEM_ASYNC_SERVICE_START	26
#define	TR_KMEM_ASYNC_SERVICE_END	27
#define	TR_KMEM_HASH_RESCALE_START	28
#define	TR_KMEM_HASH_RESCALE_END	29

/*
 * TR_FAC_STREAMS_FR tags
 */

#define	TR_STROPEN		0
#define	TR_STRCLOSE		1
#define	TR_STRCLEAN		2
#define	TR_STRREAD_ENTER	3
#define	TR_STRREAD_WAIT		4
#define	TR_STRREAD_DONE		5
#define	TR_STRREAD_AWAKE	6
#define	TR_STRRPUT_ENTER	7
#define	TR_STRRPUT_PROTERR	8
#define	TR_STRRPUT_WAKE		9
#define	TR_STRRPUT_WAKE2	10
#define	TR_STRWSRV		11
#define	TR_IOCTL_ENTER		12
#define	TR_I_CANT_FIND		13
#define	TR_I_PUSH		14
#define	TR_I_POP		15
#define	TR_I_LINK		16
#define	TR_STPDOWN		17
#define	TR_I_UNLINK		18
#define	TR_STRDOIOCTL		19
#define	TR_STRDOIOCTL_WAIT	20
#define	TR_STRDOIOCTL_PUT	21
#define	TR_STRDOIOCTL_WAIT2	22
#define	TR_STRDOIOCTL_ACK	23
#define	TR_STRSENDSIG		24
#define	TR_QATTACH_FLAGS	25
#define	TR_STRTIME		26
#define	TR_STR2TIME		27
#define	TR_STR3TIME		28
/* Unused			29 */
/* Unused 			30 */
#define	TR_STRWAITQ_TIME	31
#define	TR_STRWAITQ_WAIT2	32
#define	TR_STRWAITQ_INTR2	33
#define	TR_STRWAITQ_WAKE2	34
#define	TR_QRUN_START		35
#define	TR_QRUN_DONE		36
#define	TR_DQ_SERVICE		37
#define	TR_RMV_QP		38
#define	TR_QRUNSERVICE_START	39
#define	TR_BACKGROUND_DQ	40
#define	TR_BACKGROUND_DONE	41
#define	TR_SENDSIG		42
#define	TR_INSERTQ		43
#define	TR_REMOVEQ		44
#define	TR_DRAIN_SYNCQ_PUT	45
#define	TR_FILL_SYNCQ		46
#define	TR_CANPUT_IN    	47
#define	TR_CANPUT_OUT   	48
#define	TR_BCANPUT_IN   	49
#define	TR_BCANPUT_OUT  	50
#define	TR_STRWRITE_IN		51
#define	TR_STRWRITE_OUT		52
#define	TR_STRWRITE_WAIT	53
#define	TR_STRWRITE_WAKE	54
#define	TR_STRWRITE_PUT		55
#define	TR_STRWRITE_RESID	56
#define	TR_STRPUTMSG_IN		57
#define	TR_STRPUTMSG_WAIT	58
#define	TR_STRPUTMSG_WAKE	59
#define	TR_STRPUTMSG_OUT	60
#define	TR_QENABLE		61
#define	TR_QRUNFLAG		62
#define	TR_RUNQUEUES		63

#define	TR_QRUN_DQ		64
#define	TR_QRUNSERVICE_END	65
#define	TR_BACKGROUND_AWAKE	66
#define	TR_QRUN_LEAVES		67
#define	TR_PUT_START		68
#define	TR_PUT_END		69
#define	TR_PUTNEXT_START	70
#define	TR_PUTNEXT_END		71
#define	TR_DRAIN_SYNCQ_START	72
#define	TR_DRAIN_SYNCQ_END	73

#define	TR_STRGETMSG_ENTER	74
#define	TR_STRGETMSG_WAIT	75
#define	TR_STRGETMSG_DONE	76
#define	TR_STRGETMSG_AWAKE	77

#define	TR_KSTRGETMSG_ENTER	78
#define	TR_KSTRGETMSG_WAIT	79
#define	TR_KSTRGETMSG_DONE	80
#define	TR_KSTRGETMSG_AWAKE	81

#define	TR_KSTRPUTMSG_IN	82
#define	TR_KSTRPUTMSG_WAIT	83
#define	TR_KSTRPUTMSG_WAKE	84
#define	TR_KSTRPUTMSG_OUT	85

#define	TR_CANPUTNEXT_IN	86
#define	TR_CANPUTNEXT_OUT	87

/*
 * TR_FAC_STREAMS_MOD tags
 */

#define	TR_MI_TIMER_RSRV_IN	0
#define	TR_MI_TIMER_RSRV_OUT	1
#define	TR_MI_TIMER_FIRE	2
#define	TR_MCOPYMSG		3

/*
 * TR_FAC_SOCKMOD tags
 */

#define	TR_SOCKMOD_RPUT_IN	0
#define	TR_SOCKMOD_RPUT_OUT	1
#define	TR_SOCKMOD_WPUT_IN	2
#define	TR_SOCKMOD_WPUT_OUT	3
#define	TR_SOCKMOD_RSRV_IN	4
#define	TR_SOCKMOD_RSRV_OUT	5
#define	TR_SOCKMOD_WSRV_IN	6
#define	TR_SOCKMOD_WSRV_OUT	7

/*
 * TR_FAC_TCP tags
 */

#define	TR_TCP_OPEN		0
#define	TR_TCP_CLOSE		1
#define	TR_TCP_RPUT_IN		2
#define	TR_TCP_RPUT_OUT		3
#define	TR_TCP_WPUT_IN		4
#define	TR_TCP_WPUT_OUT		5
#define	TR_TCP_RSRV_IN		6
#define	TR_TCP_RSRV_OUT		7
#define	TR_TCP_WSRV_IN		8
#define	TR_TCP_WSRV_OUT		9
#define	TR_TCP_WPUT_SLOW_IN	10
#define	TR_TCP_WPUT_SLOW_OUT	11
#define	TR_TCP_RPUT_SLOW_IN	12
#define	TR_TCP_RPUT_SLOW_OUT	13

/*
 * TR_FAC_UDP tags
 */

#define	TR_UDP_OPEN		0
#define	TR_UDP_CLOSE		1
#define	TR_UDP_RPUT_START	2
#define	TR_UDP_RPUT_END		3
#define	TR_UDP_WPUT_START	4
#define	TR_UDP_WPUT_END		5
#define	TR_UDP_WPUT_OTHER_START	6
#define	TR_UDP_WPUT_OTHER_END	7

/*
 * TR_FAC_IP tags
 */

#define	TR_IP_OPEN		0
#define	TR_IP_CLOSE		1
#define	TR_IP_RPUT_START	2
#define	TR_IP_RPUT_END		3
#define	TR_IP_WPUT_START	4
#define	TR_IP_WPUT_END		5
#define	TR_IP_WSRV_START	6
#define	TR_IP_WSRV_END		7
#define	TR_IP_LRPUT_START	8
#define	TR_IP_LRPUT_END		9
#define	TR_IP_LWPUT_START	10
#define	TR_IP_LWPUT_END		11
#define	TR_IP_RPUT_LOCL_START	12
#define	TR_IP_RPUT_LOCL_END	13
#define	TR_IP_RPUT_LOCL_ERR	14
#define	TR_IP_RSRV_START	15
#define	TR_IP_RSRV_END		16
#define	TR_IP_CKSUM_START	17
#define	TR_IP_CKSUM_END		18
#define	TR_IP_CKSUM_COPY_START	19
#define	TR_IP_CKSUM_COPY_END	20
#define	TR_IP_WPUT_IRE_START	21
#define	TR_IP_WPUT_IRE_END	22
#define	TR_IP_WPUT_FRAG_START	23
#define	TR_IP_WPUT_FRAG_END	24
#define	TR_IP_WPUT_LOCAL_START	25
#define	TR_IP_WPUT_LOCAL_END	26

/*
 * TR_FAC_ARP tags
 */

#define	TR_ARP_OPEN		0
#define	TR_ARP_CLOSE		1
#define	TR_ARP_RPUT_START	2
#define	TR_ARP_RPUT_END		3
#define	TR_ARP_WPUT_START	4
#define	TR_ARP_WPUT_END		5
#define	TR_ARP_WSRV_START	6
#define	TR_ARP_WSRV_END		7

/*
 * TR_FAC_LE tags
 */

#define	TR_LE_OPEN		0
#define	TR_LE_CLOSE		1
#define	TR_LE_WPUT_START	2
#define	TR_LE_WPUT_END		3
#define	TR_LE_WSRV_START	4
#define	TR_LE_WSRV_END		5
#define	TR_LE_START_START	6
#define	TR_LE_START_END		7
#define	TR_LE_INTR_START	8
#define	TR_LE_INTR_END		9
#define	TR_LE_READ_START	10
#define	TR_LE_READ_END		11
#define	TR_LE_SENDUP_START	12
#define	TR_LE_SENDUP_END	13
#define	TR_LE_ADDUDIND_START	14
#define	TR_LE_ADDUDIND_END	15
#define	TR_LE_GETBUF_START	16
#define	TR_LE_GETBUF_END	17
#define	TR_LE_FREEBUF_START	18
#define	TR_LE_FREEBUF_END	19
#define	TR_LE_PROTO_START	20
#define	TR_LE_PROTO_END		21
#define	TR_LE_INIT_START	22
#define	TR_LE_INIT_END		23
#define	TR_LE_PROTO_IN		24
#define	TR_LE_PROTO_OUT		25

/*
 * TR_FAC_QE tags
 */

#define	TR_QE_OPEN		0
#define	TR_QE_CLOSE		1
#define	TR_QE_WPUT_START	2
#define	TR_QE_WPUT_END		3
#define	TR_QE_WSRV_START	4
#define	TR_QE_WSRV_END		5
#define	TR_QE_START_START	6
#define	TR_QE_START_END		7
#define	TR_QE_INTR_START	8
#define	TR_QE_INTR_END		9
#define	TR_QE_READ_START	10
#define	TR_QE_READ_END		11
#define	TR_QE_SENDUP_START	12
#define	TR_QE_SENDUP_END	13
#define	TR_QE_ADDUDIND_START	14
#define	TR_QE_ADDUDIND_END	15
#define	TR_QE_GETBUF_START	16
#define	TR_QE_GETBUF_END	17
#define	TR_QE_FREEBUF_START	18
#define	TR_QE_FREEBUF_END	19
#define	TR_QE_PROTO_START	20
#define	TR_QE_PROTO_END		21
#define	TR_QE_INIT_START	22
#define	TR_QE_INIT_END		23
#define	TR_QE_PROTO_IN		24
#define	TR_QE_PROTO_OUT		25

/*
 * TR_FAC_BE tags
 */

#define	TR_BE_OPEN		0
#define	TR_BE_CLOSE		1
#define	TR_BE_WPUT_START	2
#define	TR_BE_WPUT_END		3
#define	TR_BE_WSRV_START	4
#define	TR_BE_WSRV_END		5
#define	TR_BE_START_START	6
#define	TR_BE_START_END		7
#define	TR_BE_INTR_START	8
#define	TR_BE_INTR_END		9
#define	TR_BE_READ_START	10
#define	TR_BE_READ_END		11
#define	TR_BE_SENDUP_START	12
#define	TR_BE_SENDUP_END	13
#define	TR_BE_ADDUDIND_START	14
#define	TR_BE_ADDUDIND_END	15
#define	TR_BE_GETBUF_START	16
#define	TR_BE_GETBUF_END	17
#define	TR_BE_FREEBUF_START	18
#define	TR_BE_FREEBUF_END	19
#define	TR_BE_PROTO_START	20
#define	TR_BE_PROTO_END		21
#define	TR_BE_INIT_START	22
#define	TR_BE_INIT_END		23
#define	TR_BE_PROTO_IN		24
#define	TR_BE_PROTO_OUT		25

/*
 * TR_FAC_AE tags
 */

#define	TR_AE_OPEN		0
#define	TR_AE_CLOSE		1
#define	TR_AE_WPUT_START	2
#define	TR_AE_WPUT_END		3
#define	TR_AE_WSRV_START	4
#define	TR_AE_WSRV_END		5
#define	TR_AE_START_START	6
#define	TR_AE_START_END		7
#define	TR_AE_INTR_START	8
#define	TR_AE_INTR_END		9
#define	TR_AE_READ_START	10
#define	TR_AE_READ_END		11
#define	TR_AE_SENDUP_START	12
#define	TR_AE_SENDUP_END	13
#define	TR_AE_ADDUDIND_START	14
#define	TR_AE_ADDUDIND_END	15
#define	TR_AE_GETBUF_START	16
#define	TR_AE_GETBUF_END	17
#define	TR_AE_FREEBUF_START	18
#define	TR_AE_FREEBUF_END	19
#define	TR_AE_PROTO_START	20
#define	TR_AE_PROTO_END		21
#define	TR_AE_INIT_START	22
#define	TR_AE_INIT_END		23
#define	TR_AE_PROTO_IN		24
#define	TR_AE_PROTO_OUT		25

/*
 * TR_FAC_PHYSIO
 */
#define	TR_PHYSIO_START			0
#define	TR_PHYSIO_LOCK_START		1
#define	TR_PHYSIO_LOCK_END		2
#define	TR_PHYSIO_UNLOCK_START		3
#define	TR_PHYSIO_UNLOCK_END		4
#define	TR_PHYSIO_GETBUF_START		5
#define	TR_PHYSIO_GETBUF_END		6
#define	TR_PHYSIO_END			7
#define	TR_PHYSIO_AS_LOCK_START		8
#define	TR_PHYSIO_SEG_LOCK_START	9
#define	TR_PHYSIO_SEG_LOCK_END		10
#define	TR_PHYSIO_AS_FAULT_START	11
#define	TR_PHYSIO_AS_LOCK_END		12
#define	TR_PHYSIO_AS_UNLOCK_START	13
#define	TR_PHYSIO_SEG_UNLOCK_START	14
#define	TR_PHYSIO_AS_UNLOCK_END		15

#define	TR_PHYSIO_SEGVN_START		16
#define	TR_PHYSIO_SEGVN_UNLOCK_END	17
#define	TR_PHYSIO_SEGVN_HIT_END		18
#define	TR_PHYSIO_SEGVN_FILL_END	19
#define	TR_PHYSIO_SEGVN_MISS_END	20

/*
 * TR_FAC_META
 */
#define	TR_META_WRITE_START			0
#define	TR_META_WRITE_END			1
#define	TR_META_READ_START			2
#define	TR_META_READ_END			3
#define	TR_META_STRATEGY_START			4
#define	TR_META_STRATEGY_END			5

#define	TR_STRIPE_STRATEGY_START		7
#define	TR_STRIPE_STRATEGY_END			8
#define	TR_STRIPE_STRATEGY_CHKBUF_START		9
#define	TR_STRIPE_STRATEGY_CHKBUF_END		10
#define	TR_STRIPE_STRATEGY_BPMAPIN_START	11
#define	TR_STRIPE_STRATEGY_BPMAPIN_END		12
#define	TR_STRIPE_STRATEGY_ALLOC_START		13
#define	TR_STRIPE_STRATEGY_ALLOC_END		14
#define	TR_STRIPE_STRATEGY_MAPBUF_START		15
#define	TR_STRIPE_STRATEGY_MAPBUF_END		16
#define	TR_STRIPE_STRATEGY_STRIPE_START		17
#define	TR_STRIPE_STRATEGY_STRIPE_END		18
#define	TR_STRIPE_STRATEGY_CALLDRV_START	19
#define	TR_STRIPE_STRATEGY_CALLDRV_END		20

/*
 * TR_FAC_IPI tags
 */

#define	TR_IPI_START		0
#define	TR_IPI_RESET_SLAVE	1
#define	TR_IPI_RESET_CHAN	2
#define	TR_IPI_RESET_BOARD	3
#define	TR_IPI_TEST_BOARD	4
#define	TR_IPI_GET_XFER_MODE	5
#define	TR_IPI_SET_XFER_MODE	6
#define	TR_IPI_INTR_1		7
#define	TR_IPI_INTR_2		8
#define	TR_IPI_POLL_1		9
#define	TR_IPI_POLL_2		10
#define	TR_IPI_RETRY		11
#define	TR_IPI_IS_CMD		12
#define	TR_IPI_INTR_OK		13
#define	TR_IPI_INTR_RESP	14
#define	TR_IPI_INTR_ERR		15

/*
 * TR_FAC_IA tags
 */

#define	TR_PID_ON		0
#define	TR_PID_OFF		1
#define	TR_GROUP_ON		2
#define	TR_GROUP_OFF		3
#define	TR_CONTROL_TTY		4
#define	TR_ACTIVE_CHAIN		5

/*
 * TR_FAC_S5 tags
 */

#define	TR_S5_SYNCIP_START	0
#define	TR_S5_SYNCIP_END	1
#define	TR_S5_OPEN		2
#define	TR_S5_CLOSE		3
#define	TR_S5_READ_START	4
#define	TR_S5_READ_END		5
#define	TR_S5_WRITE_START	6
#define	TR_S5_WRITE_END		7
#define	TR_S5_RWIP_START	8
#define	TR_S5_RWIP_END		9
#define	TR_S5_GETATTR_START	10
#define	TR_S5_GETATTR_END	11
#define	TR_S5_SETATTR_START	12
#define	TR_S5_SETATTR_END	13
#define	TR_S5_ACCESS_START	14
#define	TR_S5_ACCESS_END	15
#define	TR_S5_READLINK_START	16
#define	TR_S5_READLINK_END	17
#define	TR_S5_FSYNC_START	18
#define	TR_S5_FSYNC_END		19
#define	TR_S5_LOOKUP_START	20
#define	TR_S5_LOOKUP_END	21
#define	TR_S5_CREATE_START	22
#define	TR_S5_CREATE_END	23
#define	TR_S5_REMOVE_START	24
#define	TR_S5_REMOVE_END	25
#define	TR_S5_LINK_START	26
#define	TR_S5_LINK_END		27
#define	TR_S5_RENAME_START	28
#define	TR_S5_RENAME_END	29
#define	TR_S5_MKDIR_START	30
#define	TR_S5_MKDIR_END		31
#define	TR_S5_RMDIR_START	32
#define	TR_S5_RMDIR_END		33
#define	TR_S5_READDIR_START	34
#define	TR_S5_READDIR_END	35
#define	TR_S5_SYMLINK_START	36
#define	TR_S5_SYMLINK_END	37
#define	TR_S5_GETPAGE_START	38
#define	TR_S5_GETPAGE_END	39
#define	TR_S5_PUTPAGE_START	40
#define	TR_S5_PUTPAGE_END	41
#define	TR_S5_PUTAPAGE_START	42
#define	TR_S5_PUTAPAGE_END	43
#define	TR_S5_STARTIO_START	44
#define	TR_S5_STARTIO_END	45
#define	TR_S5_MAP_START		46
#define	TR_S5_MAP_END		47

/*
 * TR_FAC_SCSI tags
 */

#define	TR_ESPSVC_ACTION_CALL			0

#define	TR_ESPSVC_START				1
#define	TR_ESPSVC_END				2
#define	TR_ESP_CALLBACK_START			3
#define	TR_ESP_CALLBACK_END			4
#define	TR_ESP_DOPOLL_START			5
#define	TR_ESP_DOPOLL_END			6
#define	TR_ESP_FINISH_START			7
#define	TR_ESP_FINISH_END			8
#define	TR_ESP_FINISH_SELECT_START		9
#define	TR_ESP_FINISH_SELECT_RESET1_END		10
#define	TR_ESP_FINISH_SELECT_RETURN1_END	11
#define	TR_ESP_FINISH_SELECT_RETURN2_END	12
#define	TR_ESP_FINISH_SELECT_FINISH_END		13
#define	TR_ESP_FINISH_SELECT_ACTION1_END	14
#define	TR_ESP_FINISH_SELECT_ACTION2_END	15
#define	TR_ESP_FINISH_SELECT_RESET2_END		16
#define	TR_ESP_FINISH_SELECT_RESET3_END		17
#define	TR_ESP_FINISH_SELECT_ACTION3_END	18
#define	TR_ESP_HANDLE_CLEARING_START		19
#define	TR_ESP_HANDLE_CLEARING_END		20
#define	TR_ESP_HANDLE_CLEARING_FINRST_END	21
#define	TR_ESP_HANDLE_CLEARING_RETURN1_END	22
#define	TR_ESP_HANDLE_CLEARING_ABORT_END	23
#define	TR_ESP_HANDLE_CLEARING_LINKED_CMD_END	24
#define	TR_ESP_HANDLE_CLEARING_RETURN2_END	25
#define	TR_ESP_HANDLE_CLEARING_RETURN3_END	26
#define	TR_ESP_HANDLE_CMD_START_START		27
#define	TR_ESP_HANDLE_CMD_START_END		28
#define	TR_ESP_HANDLE_CMD_START_ABORT_CMD_END	29
#define	TR_ESP_HANDLE_CMD_DONE_START		30
#define	TR_ESP_HANDLE_CMD_DONE_END		31
#define	TR_ESP_HANDLE_CMD_DONE_ABORT1_END	32
#define	TR_ESP_HANDLE_CMD_DONE_ABORT2_END	33
#define	TR_ESP_HANDLE_C_CMPLT_START		34
#define	TR_ESP_HANDLE_C_CMPLT_FINRST_END	35
#define	TR_ESP_HANDLE_C_CMPLT_RETURN1_END	36
#define	TR_ESP_HANDLE_C_CMPLT_ACTION1_END	37
#define	TR_ESP_HANDLE_C_CMPLT_ACTION2_END	38
#define	TR_ESP_HANDLE_C_CMPLT_ACTION3_END	39
#define	TR_ESP_HANDLE_C_CMPLT_ACTION4_END	40
#define	TR_ESP_HANDLE_C_CMPLT_RETURN2_END	41
#define	TR_ESP_HANDLE_C_CMPLT_ACTION5_END	42
#define	TR_ESP_HANDLE_C_CMPLT_PHASEMANAGE_END	43
#define	TR_ESP_HANDLE_DATA_START		44
#define	TR_ESP_HANDLE_DATA_END			45
#define	TR_ESP_HANDLE_DATA_ABORT1_END		46
#define	TR_ESP_HANDLE_DATA_ABORT2_END		47
#define	TR_ESP_HANDLE_DATA_ABORT3_END		48
#define	TR_ESP_HANDLE_DATA_DONE_START		49
#define	TR_ESP_HANDLE_DATA_DONE_END		50
#define	TR_ESP_HANDLE_DATA_DONE_RESET_END	51
#define	TR_ESP_HANDLE_DATA_DONE_PHASEMANAGE_END	52
#define	TR_ESP_HANDLE_DATA_DONE_ACTION1_END	53
#define	TR_ESP_HANDLE_DATA_DONE_ACTION2_END	54
#define	TR_ESP_HANDLE_MORE_MSGIN_START		55
#define	TR_ESP_HANDLE_MORE_MSGIN_RETURN1_END	56
#define	TR_ESP_HANDLE_MORE_MSGIN_RETURN2_END	57
#define	TR_ESP_HANDLE_MSG_IN_START		58
#define	TR_ESP_HANDLE_MSG_IN_END		59
#define	TR_ESP_HANDLE_MSG_IN_DONE_START		60
#define	TR_ESP_HANDLE_MSG_IN_DONE_FINRST_END	61
#define	TR_ESP_HANDLE_MSG_IN_DONE_RETURN1_END	62
#define	TR_ESP_HANDLE_MSG_IN_DONE_PHASEMANAGE_END	63
#define	TR_ESP_HANDLE_MSG_IN_DONE_SNDMSG_END	64
#define	TR_ESP_HANDLE_MSG_IN_DONE_ACTION_END	65
#define	TR_ESP_HANDLE_MSG_IN_DONE_RETURN2_END	66
#define	TR_ESP_HANDLE_MSG_OUT_START		67
#define	TR_ESP_HANDLE_MSG_OUT_END		68
#define	TR_ESP_HANDLE_MSG_OUT_PHASEMANAGE_END	69
#define	TR_ESP_HANDLE_MSG_OUT_DONE_START	70
#define	TR_ESP_HANDLE_MSG_OUT_DONE_END		71
#define	TR_ESP_HANDLE_MSG_OUT_DONE_FINISH_END	72
#define	TR_ESP_HANDLE_MSG_OUT_DONE_PHASEMANAGE_END	73
#define	TR_ESP_HANDLE_SELECTION_START		74
#define	TR_ESP_HANDLE_SELECTION_END		75
#define	TR_ESP_HANDLE_UNKNOWN_START		76
#define	TR_ESP_HANDLE_UNKNOWN_END		77
#define	TR_ESP_HANDLE_UNKNOWN_INT_DISCON_END	78
#define	TR_ESP_HANDLE_UNKNOWN_PHASE_DATA_END	79
#define	TR_ESP_HANDLE_UNKNOWN_PHASE_MSG_OUT_END	80
#define	TR_ESP_HANDLE_UNKNOWN_PHASE_MSG_IN_END	81
#define	TR_ESP_HANDLE_UNKNOWN_PHASE_STATUS_END	82
#define	TR_ESP_HANDLE_UNKNOWN_PHASE_CMD_END	83
#define	TR_ESP_HANDLE_UNKNOWN_RESET_END		84
#define	TR_ESP_HDATAD_START			85
#define	TR_ESP_HDATAD_END			86
#define	TR_ESP_HDATA_START			87
#define	TR_ESP_HDATA_END			88
#define	TR_ESP_ISTART_START			89
#define	TR_ESP_ISTART_END			90

#define	TR_ESP_PHASEMANAGE_CALL			91

#define	TR_ESP_PHASEMANAGE_START		92
#define	TR_ESP_PHASEMANAGE_END			93
#define	TR_ESP_POLL_START			94
#define	TR_ESP_POLL_END				95
#define	TR_ESP_RECONNECT_START			96
#define	TR_ESP_RECONNECT_F1_END			97
#define	TR_ESP_RECONNECT_RETURN1_END		98
#define	TR_ESP_RECONNECT_F2_END			99
#define	TR_ESP_RECONNECT_PHASEMANAGE_END	100
#define	TR_ESP_RECONNECT_F3_END			101
#define	TR_ESP_RECONNECT_RESET1_END		102
#define	TR_ESP_RECONNECT_RESET2_END		103
#define	TR_ESP_RECONNECT_RESET3_END		104
#define	TR_ESP_RECONNECT_SEARCH_END		105
#define	TR_ESP_RECONNECT_RESET4_END		106
#define	TR_ESP_RECONNECT_RETURN2_END		107
#define	TR_ESP_RECONNECT_RESET5_END		108
#define	TR_ESP_RUNPOLL_START			109
#define	TR_ESP_RUNPOLL_END			110
#define	TR_ESP_SCSI_IMPL_PKTALLOC_START		111
#define	TR_ESP_SCSI_IMPL_PKTALLOC_END		112
#define	TR_ESP_SCSI_IMPL_PKTFREE_START		113
#define	TR_ESP_SCSI_IMPL_PKTFREE_END		114
#define	TR_ESP_STARTCMD_START			115
#define	TR_ESP_STARTCMD_END			116
#define	TR_ESP_STARTCMD_RE_SELECTION_END	117
#define	TR_ESP_STARTCMD_ALLOC_TAG1_END		118
#define	TR_ESP_STARTCMD_ALLOC_TAG2_END		119

#define	TR_ESP_STARTCMD_PREEMPT_CALL		120

#define	TR_ESP_START_START			121
#define	TR_ESP_START_END			122
#define	TR_ESP_START_PREPARE_PKT_END		123
#define	TR_ESP_WATCH_START			124
#define	TR_ESP_WATCH_END			125
#define	TR_MAKE_SD_CMD_START			126
#define	TR_MAKE_SD_CMD_END			127
#define	TR_MAKE_SD_CMD_NO_PKT_ALLOCATED1_END	128
#define	TR_MAKE_SD_CMD_NO_PKT_ALLOCATED2_END	129
#define	TR_MAKE_SD_CMD_G0_START			130
#define	TR_MAKE_SD_CMD_G0_END			131
#define	TR_MAKE_SD_CMD_G0_SBUF_START		132
#define	TR_MAKE_SD_CMD_G0_SBUF_END		133
#define	TR_MAKE_SD_CMD_G1_START			134
#define	TR_MAKE_SD_CMD_G1_END			135
#define	TR_MAKE_SD_CMD_INIT_PKT_START		136
#define	TR_MAKE_SD_CMD_INIT_PKT_END		137
#define	TR_MAKE_SD_CMD_INIT_PKT_SBUF_START	138
#define	TR_MAKE_SD_CMD_INIT_PKT_SBUF_END	139
#define	TR_MAKE_SD_CMD_RA_START			140
#define	TR_MAKE_SD_CMD_RA_END			141

#define	TR_SDDONE_BIODONE_CALL			142

#define	TR_SDDONE_START				143
#define	TR_SDDONE_END				144
#define	TR_SDINTR_START				145
#define	TR_SDINTR_END				146
#define	TR_SDINTR_COMMAND_DONE_END		147
#define	TR_SDINTR_QUE_COMMAND_END		148
#define	TR_SDINTR_QUE_FAILED_END		149
#define	TR_SDRUNOUT_START			150
#define	TR_SDRUNOUT_END				151
#define	TR_SDSTART_START			152
#define	TR_SDSTART_END				153
#define	TR_SDSTART_NO_WORK_END			154
#define	TR_SDSTART_NO_RESOURCES_END		155
#define	TR_SDSTRATEGY_START			156
#define	TR_SDSTRATEGY_END			157
#define	TR_SDSTRATEGY_DISKSORT_START		158
#define	TR_SDSTRATEGY_DISKSORT_END		159
#define	TR_SDSTRATEGY_SDSTART_START		160
#define	TR_SDSTRATEGY_SDSTART_END		161
#define	TR_SD_CHECK_ERROR_START			162
#define	TR_SD_CHECK_ERROR_QUE_COMMAND_END	163
#define	TR_SD_CHECK_ERROR_END			164
#define	TR__ESP_START_START			165
#define	TR__ESP_START_END			166
#define	TR_ESP_INIT_CMD_START			167
#define	TR_ESP_INIT_CMD_END			168
#define	TR_ESP_EMPTY_STARTQ_START		169
#define	TR_ESP_EMPTY_STARTQ_END			170
#define	TR_SDSTRATEGY_SMALL_WINDOW_START	171
#define	TR_SDSTRATEGY_SMALL_WINDOW_END		172
#define	TR_SDSTART_SMALL_WINDOW_START		173
#define	TR_SDSTART_SMALL_WINDOW_END		174
#define	TR_ESP_USTART_START			175
#define	TR_ESP_USTART_END			176
#define	TR_ESP_USTART_NOT_FOUND_END		177
#define	TR_ESP_USTART_DEFAULT_END		178
#define	TR_ESP_PREPARE_PKT_START		179
#define	TR_ESP_PREPARE_PKT_TRAN_BADPKT_END	180
#define	TR_ESP_PREPARE_PKT_TRAN_ACCEPT_END	181
#define	TR_ESP_ALLOC_TAG_START			182
#define	TR_ESP_ALLOC_TAG_END			183
#define	TR_ESP_CALL_PKT_COMP_START		184
#define	TR_ESP_CALL_PKT_COMP_END		185
#define	TR_ESP_CALL_PKT_COMP_RETURN1_END	186
#define	TR_ESP_CALL_PKT_COMP_RETURN2_END	187
#define	TR_ESP_POLL_LOOP_START			188
#define	TR_ESP_POLL_LOOP_END			189
#define	TR_ESP_POLL_SUN4D_START			190
#define	TR_ESP_POLL_SUN4D_END			191
#define	TR_ESP_SCSI_IMPL_DMAFREE_START		192
#define	TR_ESP_SCSI_IMPL_DMAFREE_END		193

#define	TR_SDWRITE_START			194
#define	TR_SDWRITE_END				195
#define	TR_SDREAD_START				196
#define	TR_SDREAD_END				197



/*
 * TR_FAC_SCSI_ISP tags
 */
#define	TR_ISP_SCSI_GETCAP_START	1
#define	TR_ISP_SCSI_GETCAP_END		2
#define	TR_ISP_SCSI_SETCAP_START	3
#define	TR_ISP_SCSI_SETCAP_END		4
#define	TR_ISP_SCAN_TIMEOUT_START	5
#define	TR_ISP_SCAN_TIMEOUT_END		6
#define	TR_ISP_SCSI_START_START		7
#define	TR_ISP_SCSI_START_DMA_START	8
#define	TR_ISP_SCSI_START_DMA_END	9
#define	TR_ISP_SCSI_START_END		10
#define	TR_ISP_I_START_CMD_START	11
#define	TR_ISP_I_START_CMD_Q_FULL_END	12
#define	TR_ISP_I_START_CMD_SLOT_ALLOC_START	13
#define	TR_ISP_I_START_CMD_SLOT_ALLOC_END	14
#define	TR_ISP_I_START_CMD_END		15
#define	TR_ISP_I_RUN_POLLED_CMD_START	16
#define	TR_ISP_I_RUN_POLLED_CMD_END	17
#define	TR_ISP_INTR_START		18
#define	TR_ISP_INTR_NO_INTR_END		19
#define	TR_ISP_INTR_EVENT_END		20
#define	TR_ISP_INTR_Q_START		21
#define	TR_ISP_INTR_INVALID_END		22
#define	TR_ISP_INTR_PKT_START		23
#define	TR_ISP_INTR_PKT_END		24
#define	TR_ISP_INTR_Q_END		25
#define	TR_ISP_INTR_END			26
#define	TR_ISP_I_FLAG_EVENT_START	27
#define	TR_ISP_I_FLAG_EVENT_IGNORE_END	28
#define	TR_ISP_I_FLAG_EVENT_END		29
#define	TR_ISP_I_EVENT_START		30
#define	TR_ISP_I_EVENT_RETURN_END	31
#define	TR_ISP_I_EVENT_END		32
#define	TR_ISP_I_ASYNCH_EVENT_START	33
#define	TR_ISP_I_ASYNCH_EVENT_END	34
#define	TR_ISP_I_CMD_EVENT_START	35
#define	TR_ISP_I_CMD_EVENT_END		36
#define	TR_ISP_I_RESPONSE_ERROR_START	37
#define	TR_ISP_I_RESPONSE_ERROR_END	38
#define	TR_ISP_I_SEND_CMD_START		39
#define	TR_ISP_I_SEND_CMD_END		40
#define	TR_ISP_I_CALLBACK_START		41
#define	TR_ISP_I_CALLBACK_END		42
#define	TR_ISP_I_WATCH_START		43
#define	TR_ISP_I_WATCH_END		44
#define	TR_ISP_I_TIMEOUT_START		45
#define	TR_ISP_I_TIMEOUT_END		46
#define	TR_ISP_I_QFLUSH_START		47
#define	TR_ISP_I_QFLUSH_END		48
#define	TR_ISP_I_SET_MARKER_START	49
#define	TR_ISP_I_SET_MARKER_END		50
#define	TR_ISP_SCSI_ABORT_START		51
#define	TR_ISP_SCSI_ABORT_FALSE_END	52
#define	TR_ISP_SCSI_ABORT_END		53
#define	TR_ISP_SCSI_RESET_START		54
#define	TR_ISP_SCSI_RESET_FALSE_END	55
#define	TR_ISP_SCSI_RESET_END		56
#define	TR_ISP_I_RESET_INTERFACE_START	57
#define	TR_ISP_I_RESET_INTERFACE_END	58
#define	TR_ISP_I_CALL_PKT_COMP_START	59
#define	TR_ISP_I_CALL_PKT_COMP_END	60
#define	TR_ISP_I_EMPTY_WAITQ_START	61
#define	TR_ISP_I_EMPTY_WAITQ_END	62
#define	TR_ISP_I_INIT_CMD_START		63
#define	TR_ISP_SCSI_START_BIG_CDB	64
#define	TR_ISP_I_INIT_CMD_END		65
#define	TR_ISP_I_START_CMD_AFTER_SYNC	66
#define	TR_ISP_INTR_LOOP_START		67
#define	TR_ISP_INTR_LOOP_END		68
#define	TR_ISP_INTR_ASYNC_END		69
#define	TR_ISP_INTR_MBOX_END		70
#define	TR_ISP_INTR_NO_RESP_END		71
#define	TR_ISP_INTR_AGAIN		72
#define	TR_ISP_I_MBOX_CMD_COMPLETE_START	74
#define	TR_ISP_I_MBOX_CMD_COMPLETE_END		75
#define	TR_ISP_I_MBOX_CMD_START_START		76
#define	TR_ISP_I_MBOX_CMD_START_END		77
#define	TR_ISP_SCSI_PKTALLOC_START	78
#define	TR_ISP_SCSI_PKTALLOC_END	79
#define	TR_ISP_SCSI_PKTFREE_START	80
#define	TR_ISP_SCSI_PKTFREE_DONE	81
#define	TR_ISP_SCSI_PKTFREE_END		82
#define	TR_ISP_SCSI_DMAGET_START	83
#define	TR_ISP_SCSI_DMAGET_ERROR_END	84
#define	TR_ISP_SCSI_DMAGET_END		85
#define	TR_ISP_SCSI_DMAFREE_START	86
#define	TR_ISP_SCSI_DMAFREE_END		87
#define	TR_ISP_I_RESET_INIT_CHIP_START	88
#define	TR_ISP_I_RESET_INIT_CHIP_END	89


/*
 * TR_FAC_SCSI_FAS tags
 */
#define	TR_FASSVC_ACTION_CALL			1
#define	TR_FASSVC_END				2
#define	TR_FASSVC_START				3
#define	TR_FAS_ALLOC_TAG_END			4
#define	TR_FAS_ALLOC_TAG_START			5
#define	TR_FAS_DOPOLL_END			6
#define	TR_FAS_DOPOLL_START			7
#define	TR_FAS_EMPTY_WAITQ_END			8
#define	TR_FAS_EMPTY_WAITQ_START		9
#define	TR_FAS_FINISH_END			10
#define	TR_FAS_FINISH_SELECT_ACTION3_END	11
#define	TR_FAS_FINISH_SELECT_FINISH_END		12
#define	TR_FAS_FINISH_SELECT_RESET1_END		13
#define	TR_FAS_FINISH_SELECT_RESET2_END		14
#define	TR_FAS_FINISH_SELECT_RESET3_END		15
#define	TR_FAS_FINISH_SELECT_START		16
#define	TR_FAS_FINISH_START			17
#define	TR_FAS_HANDLE_CLEARING_ABORT_END	18
#define	TR_FAS_HANDLE_CLEARING_END		19
#define	TR_FAS_HANDLE_CLEARING_RETURN1_END	20
#define	TR_FAS_HANDLE_CLEARING_RETURN3_END	21
#define	TR_FAS_HANDLE_CLEARING_START		22
#define	TR_FAS_HANDLE_CMD_DONE_ABORT1_END	23
#define	TR_FAS_HANDLE_CMD_DONE_END		24
#define	TR_FAS_HANDLE_CMD_DONE_START		25
#define	TR_FAS_HANDLE_CMD_START_END		26
#define	TR_FAS_HANDLE_CMD_START_START		27
#define	TR_FAS_HANDLE_C_CMPLT_ACTION1_END	28
#define	TR_FAS_HANDLE_C_CMPLT_ACTION2_END	29
#define	TR_FAS_HANDLE_C_CMPLT_ACTION3_END	30
#define	TR_FAS_HANDLE_C_CMPLT_ACTION4_END	31
#define	TR_FAS_HANDLE_C_CMPLT_ACTION5_END	32
#define	TR_FAS_HANDLE_C_CMPLT_RETURN1_END	33
#define	TR_FAS_HANDLE_C_CMPLT_START		34
#define	TR_FAS_HANDLE_DATA_ABORT1_END		35
#define	TR_FAS_HANDLE_DATA_ABORT2_END		36
#define	TR_FAS_HANDLE_DATA_DONE_ACTION2_END	37
#define	TR_FAS_HANDLE_DATA_DONE_PHASEMANAGE_END	38
#define	TR_FAS_HANDLE_DATA_DONE_RESET_END	39
#define	TR_FAS_HANDLE_DATA_DONE_START		40
#define	TR_FAS_HANDLE_DATA_END			41
#define	TR_FAS_HANDLE_DATA_START		42
#define	TR_FAS_HANDLE_MORE_MSGIN_RETURN2_END	43
#define	TR_FAS_HANDLE_MORE_MSGIN_START		44
#define	TR_FAS_HANDLE_MSG_IN_DONE_ACTION_END	45
#define	TR_FAS_HANDLE_MSG_IN_DONE_RETURN2_END	46
#define	TR_FAS_HANDLE_MSG_IN_DONE_SNDMSG_END	47
#define	TR_FAS_HANDLE_MSG_IN_DONE_START		48
#define	TR_FAS_HANDLE_MSG_IN_END		49
#define	TR_FAS_HANDLE_MSG_IN_START		50
#define	TR_FAS_HANDLE_MSG_OUT_DONE_END		51
#define	TR_FAS_HANDLE_MSG_OUT_DONE_PHASEMANAGE_END 52
#define	TR_FAS_HANDLE_MSG_OUT_DONE_START	53
#define	TR_FAS_HANDLE_MSG_OUT_END		54
#define	TR_FAS_HANDLE_MSG_OUT_PHASEMANAGE_END	55
#define	TR_FAS_HANDLE_MSG_OUT_START		56
#define	TR_FAS_HANDLE_UNKNOWN_INT_DISCON_END	57
#define	TR_FAS_HANDLE_UNKNOWN_RESET_END		58
#define	TR_FAS_HANDLE_UNKNOWN_START		59
#define	TR_FAS_ISTART_END			60
#define	TR_FAS_ISTART_START			61
#define	TR_FAS_PHASEMANAGE_CALL			62
#define	TR_FAS_PHASEMANAGE_END			63
#define	TR_FAS_PHASEMANAGE_START		64
#define	TR_FAS_POLL_END				65
#define	TR_FAS_POLL_START			66
#define	TR_FAS_PREPARE_PKT_TRAN_ACCEPT_END	67
#define	TR_FAS_PREPARE_PKT_TRAN_BADPKT_END	68
#define	TR_FAS_RECONNECT_F2_END			69
#define	TR_FAS_RECONNECT_RESET5_END		70
#define	TR_FAS_RECONNECT_RETURN2_END		71
#define	TR_FAS_RECONNECT_START			72
#define	TR_FAS_RUNPOLL_END			73
#define	TR_FAS_RUNPOLL_START			74
#define	TR_FAS_SCSI_IMPL_DMAFREE_END		75
#define	TR_FAS_SCSI_IMPL_DMAFREE_START		76
#define	TR_FAS_SCSI_IMPL_PKTALLOC_END		77
#define	TR_FAS_SCSI_IMPL_PKTALLOC_START		78
#define	TR_FAS_SCSI_IMPL_PKTFREE_END		79
#define	TR_FAS_SCSI_IMPL_PKTFREE_START		80
#define	TR_FAS_STARTCMD_END			81
#define	TR_FAS_STARTCMD_START			82
#define	TR_FAS_START_END			83
#define	TR_FAS_START_PREPARE_PKT_END		84
#define	TR_FAS_START_START			85
#define	TR_FAS_USTART_END			86
#define	TR_FAS_USTART_NOT_FOUND_END		87
#define	TR_FAS_USTART_START			88
#define	TR_FAS_WATCH_END			89
#define	TR_FAS_SCSI_IMPL_DMAGET_END		90
#define	TR_FAS_SCSI_IMPL_DMAGET_START		91
#define	TR__FAS_START_END			92
#define	TR__FAS_START_START			93
#define	TR_FAS_RECONNECT_RETURN1_END		94
#define	TR_FAS_RECONNECT_PHASEMANAGE_END	95
#define	TR_FAS_RECONNECT_F3_END			96
#define	TR_FAS_HANDLE_UNKNOWN_PHASE_DATA_END	97
#define	TR_FAS_HANDLE_UNKNOWN_PHASE_MSG_OUT_END 98
#define	TR_FAS_HANDLE_UNKNOWN_PHASE_MSG_IN_END	99
#define	TR_FAS_HANDLE_UNKNOWN_PHASE_STATUS_END	100
#define	TR_FAS_HANDLE_UNKNOWN_PHASE_CMD_END	101
#define	TR_FAS_HANDLE_CLEARING_FINRST_END	102
#define	TR_FAS_HANDLE_CLEARING_RETURN2_END	103
#define	TR_FAS_HANDLE_DATA_DONE_ACTION1_END	104
#define	TR_FAS_HANDLE_C_CMPLT_FINRST_END	105
#define	TR_FAS_HANDLE_MORE_MSGIN_RETURN1_END	106
#define	TR_FAS_HANDLE_MSG_IN_DONE_FINRST_END	107
#define	TR_FAS_HANDLE_MSG_IN_DONE_RETURN1_END	108
#define	TR_FAS_HANDLE_MSG_IN_DONE_PHASEMANAGE_END 109
#define	TR_FAS_HANDLE_MSG_OUT_DONE_FINISH_END	110
#define	TR_FAS_EMPTY_CALLBACKQ_START		111
#define	TR_FAS_EMPTY_CALLBACKQ_END		112
#define	TR_FAS_CALL_PKT_COMP_START		113
#define	TR_FAS_CALL_PKT_COMP_END		114


/*
 * TR_FAC_LWP tags
 */

#define	TR_LWP_CREATE_START		0
#define	TR_LWP_CREATE_END		1
#define	TR_LWP_EXIT_START		2
#define	TR_LWP_EXIT_END			3
#define	TR_LWP_SUSPEND_START		4
#define	TR_LWP_SUSPEND_END		5
#define	TR_LWP_CONTINUE_START		6
#define	TR_LWP_CONTINUE_END		7

#define	TR_LWP_CREATE_ASEGKP		8
#define	TR_LWP_CREATE_ATHRC		9
#define	TR_LWP_CREATE_ABZ		10
#define	TR_LWP_CONT_ASR			11
#define	TR_LWP_EXIT_OFFQ		12

/*
 * TR_FAC_SYS_LWP tags
 */

#define	TR_SYS_LWP_CREATE_START		0
#define	TR_SYS_LWP_CREATE_END1		1
#define	TR_SYS_LWP_CREATE_END2		2
#define	TR_SYS_LWP_EXIT_START		3
#define	TR_SYS_LWP_EXIT_END		4
#define	TR_SYS_LWP_WAIT			5

#define	TR_SYS_LWP_SELF			6
#define	TR_SYS_LWP_SUSPEND_START	7
#define	TR_SYS_LWP_SUSPEND_END		8
#define	TR_SYS_LWP_CONTINUE_START	9
#define	TR_SYS_LWP_CONTINUE_END		10

#define	TR_SYS_LWP_MUTEX_LOCK		11
#define	TR_SYS_LWP_MUTEX_UNLOCK		12
#define	TR_SYS_LWP_COND_WAIT_START	13
#define	TR_SYS_LWP_COND_WAIT_END	14
#define	TR_SYS_LWP_COND_SIGNAL_START	15
#define	TR_SYS_LWP_COND_SIGNAL_END	16
#define	TR_SYS_LWP_COND_BROADCAST	17
#define	TR_SYS_LWP_KILL_START		18
#define	TR_SYS_LWP_KILL_END		19

#define	TR_SYS_LWP_SETPRIVATE		20
#define	TR_SYS_LWP_GETPRIVATE		21

#define	TR_SYS_LWP_ACOPYIN		22
#define	TR_SYS_LWP_ABCP			23
#define	TR_SYS_LWP_ALWPTOT		24
#define	TR_SYS_LWP_CONTINUE_AIDTOT	25
#define	TR_SYS_LWP_CR_WAIT		26

/*
 * TR_FAC_CALLOUT tags
 */

#define	TR_TIMEOUT			0
#define	TR_UNTIMEOUT			1
#define	TR_UNTIMEOUT_BOGUS_ID		2
#define	TR_UNTIMEOUT_EXECUTING		3
#define	TR_UNTIMEOUT_SELF		4
#define	TR_CALLOUT_START		5
#define	TR_CALLOUT_END			6

/*
 * TR_FAC_SPECFS tags
 */

#define	TR_SPECFS_GETPAGE	0
#define	TR_SPECFS_GETAPAGE	1
#define	TR_SPECFS_PUTPAGE	2
#define	TR_SPECFS_PUTAPAGE	3
#define	TR_SPECFS_SEGMAP	4
#define	TR_SPECFS_OPEN		5
#define	TR_SPECFS_READ_START	6
#define	TR_SPECFS_READ_END	7
#define	TR_SPECFS_CDEV		8
#define	TR_SPECFS_WRITE_START	9
#define	TR_SPECFS_WRITE_END	10

/*
 * TR_FAC_TMPFS tags
 */

#define	TR_TMPFS_LOOKUP		0
#define	TR_TMPFS_CREATE		1
#define	TR_TMPFS_REMOVE		2
#define	TR_TMPFS_RENAME		3
#define	TR_TMPFS_RWTMP_START	4
#define	TR_TMPFS_RWTMP_END	5

/*
 * TR_FAC_SWAPFS tags
 */

#define	TR_SWAPFS_OPEN		0
#define	TR_SWAPFS_CLOSE		1
#define	TR_SWAPFS_GETPAGE	2
#define	TR_SWAPFS_GETAPAGE	3
#define	TR_SWAPFS_PUTPAGE	4
#define	TR_SWAPFS_PUTAPAGE	5
/*
 * TR_FAC_UFS tags
 */

#define	TR_UFS_SYNCIP_START	0
#define	TR_UFS_SYNCIP_END	1
#define	TR_UFS_OPEN		2
#define	TR_UFS_CLOSE		4
#define	TR_UFS_READ_START	6
#define	TR_UFS_READ_END		7
#define	TR_UFS_WRITE_START	8
#define	TR_UFS_WRITE_END	9
#define	TR_UFS_RWIP_START	10
#define	TR_UFS_RWIP_END		11
#define	TR_UFS_GETATTR_START	12
#define	TR_UFS_GETATTR_END	13
#define	TR_UFS_SETATTR_START	14
#define	TR_UFS_SETATTR_END	15
#define	TR_UFS_ACCESS_START	16
#define	TR_UFS_ACCESS_END	17
#define	TR_UFS_READLINK_START	18
#define	TR_UFS_READLINK_END	19
#define	TR_UFS_FSYNC_START	20
#define	TR_UFS_FSYNC_END	21
#define	TR_UFS_LOOKUP_START	22
#define	TR_UFS_LOOKUP_END	23
#define	TR_UFS_CREATE_START	24
#define	TR_UFS_CREATE_END	25
#define	TR_UFS_REMOVE_START	26
#define	TR_UFS_REMOVE_END	27
#define	TR_UFS_LINK_START	28
#define	TR_UFS_LINK_END		29
#define	TR_UFS_RENAME_START	30
#define	TR_UFS_RENAME_END	31
#define	TR_UFS_MKDIR_START	32
#define	TR_UFS_MKDIR_END	33
#define	TR_UFS_RMDIR_START	34
#define	TR_UFS_RMDIR_END	35
#define	TR_UFS_READDIR_START	36
#define	TR_UFS_READDIR_END	37
#define	TR_UFS_SYMLINK_START	38
#define	TR_UFS_SYMLINK_END	39
#define	TR_UFS_GETPAGE_START	40
#define	TR_UFS_GETPAGE_END	41
#define	TR_UFS_GETAPAGE_START	42
#define	TR_UFS_GETAPAGE_END	43
#define	TR_UFS_PUTPAGE_START	44
#define	TR_UFS_PUTPAGE_END	45
#define	TR_UFS_PUTAPAGE_START	46
#define	TR_UFS_PUTAPAGE_END	47
#define	TR_UFS_MAP_START	48
#define	TR_UFS_MAP_END		49
#define	TR_UFS_GETSECATTR_START	50
#define	TR_UFS_GETSECATTR_END	51
#define	TR_UFS_SETSECATTR_START	52
#define	TR_UFS_SETSECATTR_END	53

/*
 * TR_FAC_NFS tags
 *
 *	Simple convention: client tags range from 0-99, server
 *	tags range from 100 up.
 */

#define	TR_RFSCALL_START	0
#define	TR_RFSCALL_END		1
#define	TR_FHTOVP_START		2
#define	TR_FHTOVP_END		3

#define	TR_VOP_GETATTR_START	100
#define	TR_VOP_GETATTR_END	101
#define	TR_VOP_SETATTR_START	102
#define	TR_VOP_SETATTR_END	103
#define	TR_VOP_LOOKUP_START	104
#define	TR_VOP_LOOKUP_END	105
#define	TR_VOP_READLINK_START	106
#define	TR_VOP_READLINK_END	107
#define	TR_VOP_RWLOCK_START	108
#define	TR_VOP_RWLOCK_END	109
#define	TR_VOP_ACCESS_START	110
#define	TR_VOP_ACCESS_END	111
#define	TR_VOP_OPEN_START	112
#define	TR_VOP_OPEN_END		113
#define	TR_VOP_READ_START	114
#define	TR_VOP_READ_END		115
#define	TR_VOP_CLOSE_START	116
#define	TR_VOP_CLOSE_END	117
#define	TR_VOP_RWUNLOCK_START	118
#define	TR_VOP_RWUNLOCK_END	119
#define	TR_VOP_WRITE_START	120
#define	TR_VOP_WRITE_END	121
#define	TR_VOP_CREATE_START	122
#define	TR_VOP_CREATE_END	123
#define	TR_VOP_REMOVE_START	124
#define	TR_VOP_REMOVE_END	125
#define	TR_VOP_RENAME_START	126
#define	TR_VOP_RENAME_END	127
#define	TR_VOP_LINK_START	128
#define	TR_VOP_LINK_END		129
#define	TR_VOP_SYMLINK_START	130
#define	TR_VOP_SYMLINK_END	131
#define	TR_VOP_MKDIR_START	132
#define	TR_VOP_MKDIR_END	133
#define	TR_VOP_RMDIR_START	134
#define	TR_VOP_RMDIR_END	135
#define	TR_VOP_READDIR_START	136
#define	TR_VOP_READDIR_END	137
#define	TR_SEGMAP_GETMAP_START	138
#define	TR_SEGMAP_GETMAP_END	139
#define	TR_AS_FAULT_START	140
#define	TR_AS_FAULT_END		141
#define	TR_RFS_GETATTR_START	142
#define	TR_RFS_GETATTR_END	143
#define	TR_RFS_SETATTR_START	144
#define	TR_RFS_SETATTR_END	145
#define	TR_RFS_LOOKUP_START	146
#define	TR_RFS_LOOKUP_END	147
#define	TR_RFS_READLINK_START	148
#define	TR_RFS_READLINK_END	149
#define	TR_RFS_READ_START	150
#define	TR_RFS_READ_END		151
#define	TR_RFS_WRITE_START	152
#define	TR_RFS_WRITE_END	153
#define	TR_RFS_CREATE_START	154
#define	TR_RFS_CREATE_END	155
#define	TR_RFS_REMOVE_START	156
#define	TR_RFS_REMOVE_END	157
#define	TR_RFS_RENAME_START	158
#define	TR_RFS_RENAME_END	159
#define	TR_RFS_LINK_START	160
#define	TR_RFS_LINK_END		161
#define	TR_RFS_SYMLINK_START	162
#define	TR_RFS_SYMLINK_END	163
#define	TR_RFS_MKDIR_START	164
#define	TR_RFS_MKDIR_END	165
#define	TR_RFS_RMDIR_START	166
#define	TR_RFS_RMDIR_END	167
#define	TR_RFS_READDIR_START	168
#define	TR_RFS_READDIR_END	169
#define	TR_RFS_STATFS_START	170
#define	TR_RFS_STATFS_END	171
#define	TR_SVC_SENDREPLY_START	178
#define	TR_SVC_SENDREPLY_END	179

/* More VOP calls */
#define	TR_VOP_FSYNC_START	180
#define	TR_VOP_FSYNC_END	181
#define	TR_VOP_PUTPAGE_START	182
#define	TR_VOP_PUTPAGE_END	183
#define	TR_SEGMAP_RELEASE_START	184
#define	TR_SEGMAP_RELEASE_END	185
#define	TR_SVC_GETARGS_START	186
#define	TR_SVC_GETARGS_END	187
#define	TR_FINDEXPORT_START	188
#define	TR_FINDEXPORT_END	189
#define	TR_SVC_FREEARGS_START	192
#define	TR_SVC_FREEARGS_END	193
#define	TR_RFS_RESFREE_START	194
#define	TR_RFS_RESFREE_END	195
#define	TR_RFS_CRFREE_START	196
#define	TR_RFS_CRFREE_END	197
#define	TR_SVC_DUPFOUND		198

/* NFS fast path server trace points */
#define	TR_NFSFP_QUE_REQ_START	199
#define	TR_NFSFP_QUE_REQ_END	200
#define	TR_NFSFP_PROC_REQ_END	201
#define	TR_NFSFP_XDR_ARG_START	202
#define	TR_NFSFP_XDR_ARG_END	203
#define	TR_NFSFP_XDR_RES_START	204
#define	TR_NFSFP_XDR_RES_END	205
#define	TR_NFSFP_DUP_CHECK_START	206
#define	TR_NFSFP_DUP_CHECK_END	207
#define	TR_NFSFP_UDP_SEND_START	208
#define	TR_NFSFP_UDP_SEND_END	209
#define	TR_NFSFP_SEND_REPLY_START	210
#define	TR_NFSFP_SEND_REPLY_END	211
#define	TR_NFSFP_QUE_REQ_ENQ	212
#define	TR_NFSFP_QUE_REQ_DEQ	213
#define	TR_NFSFP_RFS_READLINK_START	214
#define	TR_NFSFP_RFS_READLINK_END	215
#define	TR_NFSFP_SVCAUTH_UNIX_START	216
#define	TR_NFSFP_SVCAUTH_UNIX_END	217
#define	TR_SVC_FREERES_START	222
#define	TR_SVC_FREERES_END	223

/* Name cache tracing */
#define	TR_DNLC_ENTER_START	218
#define	TR_DNLC_ENTER_END	219
#define	TR_DNLC_LOOKUP_START	220
#define	TR_DNLC_LOOKUP_END	221

/* Common dispatch tracing */
#define	TR_CMN_DISPATCH_START	224
#define	TR_CMN_PROC_START	225
#define	TR_CMN_PROC_END		226
#define	TR_CMN_DISPATCH_END	227

/* More VOP calls */
#define	TR_VOP_SPACE_START	228
#define	TR_VOP_SPACE_END	229

/*
 * TR_FAC_DDI tags
 */

#define	TR_DDI_DMA_BUF_SETUP_START		0
#define	TR_DDI_DMA_BUF_SETUP_END		1
#define	TR_IOMMUNEX_DMA_MAP_START		2
#define	TR_IOMMUNEX_DMA_MAP_END			3
#define	TR_IOMMUNEX_DMA_MAP_GETDVMAPAGES_START	4
#define	TR_IOMMUNEX_DMA_MAP_GETDVMAPAGES_END	5

/*
 * TR_FAC_KRPC tags
 */

#define	TR_SVC_GETREQ_START		0
#define	TR_SVC_GETREQ_END		1
#define	TR_SVC_GETREQ_LOOP_START	2
#define	TR_SVC_GETREQ_LOOP_END		3
#define	TR_SVC_RUN			4
#define	TR_SVC_CLTS_KRECV_START		5
#define	TR_SVC_CLTS_KRECV_END		6
#define	TR_XDR_CALLMSG_START		7
#define	TR_XDR_CALLMSG_END		8
#define	TR_SVC_CLTS_KSEND_START		9
#define	TR_SVC_CLTS_KSEND_END		10
#define	TR_XDR_REPLYMSG_START		11
#define	TR_XDR_REPLYMSG_END		12
#define	TR_RPCMODOPEN_START		13
#define	TR_RPCMODOPEN_END		14
#define	TR_RPCMODRPUT_START		15
#define	TR_RPCMODRPUT_END		16
#define	TR_RPCMODWPUT_START		17
#define	TR_RPCMODWPUT_END		18
#define	TR_RPCMODRSRV_START		19
#define	TR_RPCMODRSRV_END		20
#define	TR_RPCMODWSRV_START		21
#define	TR_RPCMODWSRV_END		22
#define	TR_SVC_QUEUEREQ_START		23
#define	TR_SVC_QUEUEREQ_END		24
#define	TR_SVC_GETREQ_AUTH_START	25
#define	TR_SVC_GETREQ_AUTH_END		26
#define	TR_SVC_DUPDONE			27
#define	TR_SVC_CLTS_KDUP_START		28
#define	TR_SVC_CLTS_KDUP_END		29
#define	TR_RPC_QUE_REQ_START		30
#define	TR_RPC_QUE_REQ_END		31
#define	TR_SVC_COTS_KRECV_START		32
#define	TR_SVC_COTS_KRECV_END		33
#define	TR_SVC_COTS_KDUP_DONE		34
#define	TR_SVC_COTS_KDUP_INPROGRESS	35
#define	TR_SVC_COTS_KSEND_START		36
#define	TR_SVC_COTS_KSEND_END		37

/*
 * TR_FAC_SCSI_RES
 */

#define	TR_SCSI_INIT_PKT_START				0
#define	TR_SCSI_INIT_PKT_RETURN1_END			1
#define	TR_SCSI_INIT_PKT_RETURN2_END			2
#define	TR_SCSI_INIT_PKT_END				3
#define	TR_SCSI_INIT_PKT_PKTALLOC_START			4
#define	TR_SCSI_INIT_PKT_PKTALLOC_END			5
#define	TR_SCSI_INIT_PKT_PKTALLOC_FAILED		6
#define	TR_SCSI_INIT_PKT_DMAGET_START			7
#define	TR_SCSI_INIT_PKT_DMAGET_FAILED			8
#define	TR_SCSI_INIT_PKT_FREE_START			9
#define	TR_SCSI_ALLOC_CONSISTENT_BUF_START		10
#define	TR_SCSI_ALLOC_CONSISTENT_BUF_RETURN1_END	11
#define	TR_SCSI_ALLOC_CONSISTENT_BUF_RETURN2_END	12
#define	TR_SCSI_ALLOC_CONSISTENT_BUF_RETURN3_END	13
#define	TR_SCSI_ALLOC_CONSISTENT_BUF_END		14
#define	TR_SCSI_FREE_CONSISTENT_BUF_START		15
#define	TR_SCSI_FREE_CONSISTENT_BUF_END			16

#define	TR_SCSI_IMPL_PKTALLOC_START			17
#define	TR_SCSI_IMPL_PKTALLOC_END			18
#define	TR_SCSI_IMPL_PKTALLOC_CALLBACK_START		19
#define	TR_SCSI_IMPL_PKTALLOC_CALLBACK_END		20
#define	TR_SCSI_IMPL_PKTFREE_START			21
#define	TR_SCSI_IMPL_PKTFREE_END			22
#define	TR_SCSI_IMPL_PKTFREE_RUN_CALLBACK		23

#define	TR_SCSI_IMPL_DMAGET_START			24
#define	TR_SCSI_IMPL_DMAGET_END				25
#define	TR_SCSI_IMPL_DMAGET_BUFSETUP_START		26
#define	TR_SCSI_IMPL_DMAGET_BUFSETUP_FAILED		27

#define	TR_SCSI_DESTROY_PKT_START			28
#define	TR_SCSI_DESTROY_PKT_END				29

#define	TR_FIFOREAD_IN		1
#define	TR_FIFOREAD_OUT		2
#define	TR_FIFOREAD_WAIT	3
#define	TR_FIFOREAD_WAKE	4
#define	TR_FIFOREAD_STREAM	5
#define	TR_FIFOWRITE_IN		6
#define	TR_FIFOWRITE_OUT	7
#define	TR_FIFOWRITE_WAIT	9
#define	TR_FIFOWRITE_WAKE	10
#define	TR_FIFOWRITE_STREAM	11

#define	TR_RLOGINP_RPUT_IN	0
#define	TR_RLOGINP_RPUT_OUT	1
#define	TR_RLOGINP_RSRV_IN	2
#define	TR_RLOGINP_RSRV_OUT	3
#define	TR_RLOGINP_WSRV_IN	4
#define	TR_RLOGINP_WSRV_OUT	5
#define	TR_RLOGINP_WPUT_IN	6
#define	TR_RLOGINP_WPUT_OUT	7
#define	TR_RLOGINP_WINCTL_IN	8
#define	TR_RLOGINP_WINCTL_OUT	9

/*
 * TR_FAC_SOCKFS tags
 */
#define	TR_SOCKFS_OPEN		0
#define	TR_SOCKFS_CLOSE		1

/*
 * TR_FAC_DEVMAP tags
 */

#define	TR_DEVMAP_DUP			0
#define	TR_DEVMAP_UNMAP			1
#define	TR_DEVMAP_FREE			2
#define	TR_DEVMAP_FAULT			3
#define	TR_DEVMAP_FAULTA		4
#define	TR_DEVMAP_SETPROT		5
#define	TR_DEVMAP_CHECKPROT		6
#define	TR_DEVMAP_SEGDEV_BADOP		7
#define	TR_DEVMAP_SYNC			8
#define	TR_DEVMAP_INCORE		9
#define	TR_DEVMAP_LOCKOP		10
#define	TR_DEVMAP_GETPROT		11
#define	TR_DEVMAP_GETOFFSET		12
#define	TR_DEVMAP_GETTYPE		13
#define	TR_DEVMAP_GETVP			14
#define	TR_DEVMAP_ADVISE		15
#define	TR_DEVMAP_DUMP			16
#define	TR_DEVMAP_PAGELOCK		17
#define	TR_DEVMAP_GETMEMID		18
#define	TR_DEVMAP_SOFTUNLOCK		19
#define	TR_DEVMAP_FAULTPAGE		20
#define	TR_DEVMAP_FAULTPAGES		21
#define	TR_DEVMAP_SEGMAP_SETUP		22
#define	TR_DEVMAP_DEVICE		23
#define	TR_DEVMAP_DO_CTXMGT		24
#define	TR_DEVMAP_ROUNDUP		25
#define	TR_DEVMAP_FIND_HANDLE		26
#define	TR_DEVMAP_UNLOAD		27
#define	TR_DEVMAP_GET_LARGE_PGSIZE	28
#define	TR_DEVMAP_SOFTLOCK_INIT		29
#define	TR_DEVMAP_SOFTLOCK_RELE		30
#define	TR_DEVMAP_CTX_RELE		31
#define	TR_DEVMAP_LOAD			32
#define	TR_DEVMAP_SETUP			33
#define	TR_DEVMAP_SEGMAP		34
#define	TR_DEVMAP_DEVMEM_SETUP		35
#define	TR_DEVMAP_DEVMEM_REMAP		36
#define	TR_DEVMAP_UMEM_SETUP		37
#define	TR_DEVMAP_UMEM_REMAP		38
#define	TR_DEVMAP_SET_CTX_TIMEOUT	39
#define	TR_DEVMAP_DEFAULT_ACCESS	40
#define	TR_DEVMAP_UMEM_ALLOC		41
#define	TR_DEVMAP_UMEM_FREE		42
#define	TR_DEVMAP_CTXTO			43
#define	TR_DEVMAP_DUP_CK1		44
#define	TR_DEVMAP_UNMAP_CK1		45
#define	TR_DEVMAP_UNMAP_CK2		46
#define	TR_DEVMAP_UNMAP_CK3		47
#define	TR_DEVMAP_FAULT_CK1		48
#define	TR_DEVMAP_SETPROT_CK1		49
#define	TR_DEVMAP_FAULTPAGE_CK1		50
#define	TR_DEVMAP_DO_CTXMGT_CK1		51
#define	TR_DEVMAP_DO_CTXMGT_CK2		52
#define	TR_DEVMAP_DO_CTXMGT_CK3		53
#define	TR_DEVMAP_DO_CTXMGT_CK4		54
#define	TR_DEVMAP_ROUNDUP_CK1		55
#define	TR_DEVMAP_ROUNDUP_CK2		56
#define	TR_DEVMAP_CTX_RELE_CK1		57


/*
 * TR_FAC_DAD tags
 */

#define	TR_DCDSTRATEGY_START				1
#define	TR_DCDSTRATEGY_DISKSORT_START			2
#define	TR_DCDSTRATEGY_DISKSORT_END			3
#define	TR_DCDSTRATEGY_SMALL_WINDOW_START		4
#define	TR_DCDSTRATEGY_SMALL_WINDOW_END			5
#define	TR_DCDSTRATEGY_END				6
#define	TR_DCDSTART_START				7
#define	TR_DCDSTART_NO_WORK_END				8
#define	TR_DCDSTART_NO_RESOURCES_END			9
#define	TR_DCASTART_SMALL_WINDOW_START			10
#define	TR_DCDSTART_SMALL_WINDOW_END			11
#define	TR_DCDSTART_END					12
#define	TR_MAKE_DCD_CMD_START				13
#define	TR_MAKE_DCD_CMD_INIT_PKT_START			14
#define	TR_MAKE_DCD_CMD_INIT_PKT_END			15
#define	TR_MAKE_DCD_CMD_NO_PKT_ALLOCATED1_END		16
#define	TR_MAKE_DCD_CMD_END				17
#define	TR_DCDINTR_START				18
#define	TR_DCDINTR_COMMAND_DONE_END			19
#define	TR_DCDINTR_END					20
#define	TR_DCDONE_START					21
#define	TR_DCDDONE_BIODONE_CALL				22
#define	TR_DCDDONE_END					23
#define	TR_DCD_CHECK_ERROR_START			24
#define	TR_DCD_CHECK_ERROR_END				25
#define	TR_DCDRUNOUT_START				26
#define	TR_DCDRUNOUT_END				27

/*
 * Every trace file begins with a version record, so tools can tell whether
 * or not they're equipped to decode the file.  For this reason, the format
 * of the version record should *never* be changed.
 * The version consists of major, micro, and micro numbers.  They have
 * specific meanings:
 *
 *   - v_micro: existing tools should be unaffected.  This should be the case
 *		if all you do is add new trace points, for example.
 *
 *   - v_minor: existing tools may have to be recompiled, possibly with one
 *		or two very small changes.  For example, this would generally
 *		be the case if you modified one of the standard (facility 0)
 *		trace record structures.
 *
 *   - v_major: you've changed the world and broken everything.
 *
 * If you make a change to this file, please rev the version according to
 * the above guidelines.
 */

#define	VT_VERSION_MAJOR	3
#define	VT_VERSION_MINOR	0
#define	VT_VERSION_MICRO	0
#define	VT_VERSION_NAME	"92/02/18"

#define	VT_FAC_SHIFT	24
#define	VT_FAC_MASK	0xff
#define	VT_TAG_SHIFT	16
#define	VT_TAG_MASK	0xff
#define	VT_EVENT_SHIFT	16
#define	VT_EVENT_MASK	0xffff
#define	VT_TIME_SHIFT	0
#define	VT_TIME_MASK	0xffff

#define	VT_MAX_FAC	VT_FAC_MASK
#define	VT_MAX_TAG	VT_TAG_MASK
#define	VT_MAX_EVENT	VT_EVENT_MASK

#define	VT_MAX_WORDS	64
#define	VT_MAX_BYTES	256

#define	TR_DATA_MIN	TR_DATA_4
#define	TR_DATA_MAX	TR_DATA_64

#define	IS_DATA_REC(x)	(((x) >= TR_DATA_MIN) && ((x) <= TR_DATA_MAX))

/*
 * Note: the eight VT_* macros below operate on words, NOT pointers to words!
 */

#define	VT_FAC(vt)	(((vt) >> VT_FAC_SHIFT) & VT_FAC_MASK)
#define	VT_TAG(vt)	(((vt) >> VT_TAG_SHIFT) & VT_TAG_MASK)
#define	VT_EVENT(vt)	(((vt) >> VT_EVENT_SHIFT) & VT_EVENT_MASK)
#define	VT_TIME(vt)	(((vt) >> VT_TIME_SHIFT) & VT_TIME_MASK)

#define	VT_SET_FAC(vt, fac)	vt |= (fac)   << VT_FAC_SHIFT
#define	VT_SET_TAG(vt, tag)	vt |= (tag)   << VT_TAG_SHIFT
#define	VT_SET_EVENT(vt, event)	vt |= (event) << VT_EVENT_SHIFT
#define	VT_SET_TIME(vt, time)	vt |= (time)  << VT_TIME_SHIFT

#define	FTT2HEAD(fac, tag, time) \
	(((fac) << VT_FAC_SHIFT) | ((tag) << VT_TAG_SHIFT) | (time))

#define	ET2HEAD(event, time)	(((event) << VT_EVENT_SHIFT) | (time))
#define	FT2EVENT(fac, tag)	(((fac) << 8) | (tag))

/*
 * Meaning of bits in the event map
 */

#define	VT_ENABLED		0x80
#define	VT_USED			0x40
/* reserved			0x20 */
#define	VT_STRING_5		0x10
#define	VT_STRING_4		0x08
#define	VT_STRING_3		0x04
#define	VT_STRING_2		0x02
#define	VT_STRING_1		0x01

#define	VT_STRING_MASK		0x1f

#define	VT_MAPSIZE		65536

#define	VT_TEST(map, event, mask) \
	(map[event] & (mask))
#define	VT_SET(map, event, mask) \
	map[event] |= (mask)
#define	VT_UNSET(map, event, mask) \
	map[event] &= ~(mask)

#define	VT_TEST_FT(map, fac, tag, mask) \
	VT_TEST(map, FT2EVENT(fac, tag), mask)

#define	VT_SET_FT(map, fac, tag, mask) \
	VT_SET(map, FT2EVENT(fac, tag), mask)

#define	VT_UNSET_FT(map, fac, tag, mask) \
	VT_UNSET(map, FT2EVENT(fac, tag), mask)

#define	VT_FAC_ENABLE(map, fac) \
	{ \
		ulong_t xvt_i; \
		for (xvt_i = FT2EVENT((fac), 0); \
		    xvt_i < (ulong_t)FT2EVENT((fac) + 1, 0); \
		    xvt_i++) \
			map[xvt_i] = VT_ENABLED; \
	}

#ifndef _ASM

/*
 * Trace record structure definitions
 */

/*
 * Common trace header
 */

typedef ulong_t vt_trace_t;

/*
 * Generic trace record -- any trace record will fit in this struct.
 */

typedef struct vt_generic {
	vt_trace_t head;		/* common header */
	ulong_t	data[VT_MAX_WORDS - 1];	/* generic data */
} vt_generic_t;


/*
 * TR_VERSION
 */

typedef struct vt_version {
	vt_trace_t head;	/* common header */
	ulong_t	v_major;	/* major version number */
	ulong_t	v_minor;	/* minor version number */
	ulong_t	v_micro;	/* micro version number */
	ulong_t	v_name;		/* version name */
	/* followed by a data record with the version string */
} vt_version_t;

/*
 * TR_TITLE
 */

typedef struct vt_title {
	vt_trace_t head;	/* common header */
	ulong_t	title;		/* trace file title */
	/* followed by a data record with the title string */
} vt_title_t;

/*
 * TR_LABEL
 */

typedef struct vt_label {
	vt_trace_t head;	/* common header */
	ulong_t	facility;	/* facility: UFS, VM, STREAMS, ... */
	ulong_t	tag;		/* tag within the facility */
	ulong_t	length;		/* length in words */
	ulong_t	info;		/* Low-order 5 bits are the string bits */
	ulong_t	npf;		/* name-plus-format string */
	/* followed by a data record with the name\0format\0 string */
} vt_label_t;

/*
 * TR_PAGESIZE
 */

typedef struct vt_pagesize {
	vt_trace_t head;	/* common header */
	ulong_t	pagesize;	/* system page size */
} vt_pagesize_t;

/*
 * TR_NUM_CPUS
 */

typedef struct vt_num_cpus {
	vt_trace_t head;	/* common header */
	ulong_t	num_cpus;	/* number of CPUs on the system */
} vt_num_cpus_t;

/*
 * TR_CPU
 */

typedef struct vt_cpu {
	vt_trace_t head;	/* common header */
	ulong_t	cpu_num;	/* which cpu generated the following traces */
} vt_cpu_t;

/*
 * TR_DATA_4
 */

typedef struct vt_data_4 {
	vt_trace_t head;	/* common header */
	ulong_t	data[3];	/* generic data */
} vt_data_4_t;

/*
 * TR_DATA_8
 */

typedef struct vt_data_8 {
	vt_trace_t head;	/* common header */
	ulong_t	data[7];	/* generic data */
} vt_data_8_t;

/*
 * TR_DATA_16
 */

typedef struct vt_data_16 {
	vt_trace_t head;	/* common header */
	ulong_t	data[15];	/* generic data */
} vt_data_16_t;

/*
 * TR_DATA_32
 */

typedef struct vt_data_32 {
	vt_trace_t head;	/* common header */
	ulong_t	data[31];	/* generic data */
} vt_data_32_t;

/*
 * TR_DATA_64
 */

typedef struct vt_data_64 {
	vt_trace_t head;	/* common header */
	ulong_t	data[63];	/* generic data */
} vt_data_64_t;

/*
 * TR_ABS_TIME
 */

typedef struct vt_abs_time {
	vt_trace_t head;	/* common header */
	struct {
		ulong_t	hi32;	/* time.hi32 = hi32(hrtime_t) */
		ulong_t	lo32;	/* time.lo32 = lo32(hrtime_t) */
	} time;
} vt_abs_time_t;

/*
 * TR_START_TIME
 */

typedef struct vt_start_time {
	vt_trace_t head;	/* common header */
	struct {
		ulong_t	hi32;	/* time.hi32 = hi32(hrtime_t) */
		ulong_t	lo32;	/* time.lo32 = lo32(hrtime_t) */
	} time;
} vt_start_time_t;

/*
 * TR_ELAPSED_TIME
 */

typedef struct vt_elapsed_time {
	vt_trace_t head;	/* common header */
	ulong_t time;		/* 32 bit time since previous trace */
} vt_elapsed_time_t;

/*
 * TR_TOTAL_TIME
 */

typedef struct vt_total_time {
	vt_trace_t head;	/* common header */
	struct {
		ulong_t	hi32;	/* time.hi32 = hi32(hrtime_t) */
		ulong_t	lo32;	/* time.lo32 = lo32(hrtime_t) */
	} time;
} vt_total_time_t;

/*
 * TR_KTHREAD_ID
 */

typedef struct vt_kthread_id {
	vt_trace_t head;	/* common header */
	ulong_t	pid;		/* process ID */
	ulong_t	lwpid;		/* lwp ID */
	ulong_t	tid;		/* thread ID */
	ulong_t	vid;		/* vtrace ID */
	ulong_t name;		/* thread name */
	/* followed by a data record with the thread name string */
} vt_kthread_id_t;

/*
 * TR_UTHREAD_ID
 */

typedef struct vt_uthread_id {
	vt_trace_t head;	/* common header */
	ulong_t	pid;		/* process ID */
	ulong_t	lwpid;		/* lwp ID */
	ulong_t	tid;		/* thread ID */
	ulong_t	vid;		/* vtrace ID */
	ulong_t	name;		/* thread name */
	/* followed by a data record with the thread name string */
} vt_uthread_id_t;

/*
 * TR_CLOCK_FREQUENCY
 */

typedef struct vt_clock_frequency {
	vt_trace_t head;	/* common header */
	ulong_t	freq;		/* clock rate for vtrace timing */
} vt_clock_frequency_t;

/*
 * TR_RAW_KTHREAD_ID
 */

typedef struct vt_raw_kthread_id {
	vt_trace_t head;	/* common header */
	ulong_t	tid;		/* thread ID */
} vt_raw_kthread_id_t;

/*
 * TR_RAW_UTHREAD_ID
 */

typedef struct vt_raw_uthread_id {
	vt_trace_t head;	/* common header */
	ulong_t	pid;		/* process ID */
	ulong_t	lwpid;		/* lwp ID */
	ulong_t	tid;		/* thread ID */
} vt_raw_uthread_id_t;

/*
 * TR_KTHREAD_LABEL
 */

typedef struct vt_kthread_label {
	vt_trace_t head;	/* common header */
	ulong_t	pid;		/* process ID */
	ulong_t	lwpid;		/* lwp ID */
	ulong_t	tid;		/* thread ID */
	ulong_t	startpc;	/* PC where the thread started */
} vt_kthread_label_t;

/*
 * TR_UTHREAD_LABEL
 */

typedef struct vt_uthread_label {
	vt_trace_t head;	/* common header */
	ulong_t	pid;		/* process ID */
	ulong_t	lwpid;		/* lwp ID */
	ulong_t	tid;		/* thread ID */
	ulong_t	startpc;	/* PC where the thread started */
} vt_uthread_label_t;

/*
 * TR_PROCESS_NAME
 */

typedef struct vt_process_name {
	vt_trace_t head;	/* common header */
	ulong_t	pid;		/* process ID */
	ulong_t	name;		/* process name (u.u_psargs) */
	/* followed by a data record with the process name string */
} vt_process_name_t;

/*
 * TR_PROCESS_FORK
 */

typedef struct vt_process_fork {
	vt_trace_t head;	/* common header */
	ulong_t	cpid;		/* child process ID */
	ulong_t	ppid;		/* parent process ID */
} vt_process_fork_t;

/*
 * pointer to any structure in a trace file
 */

typedef union vt_pointer {
	ulong_t			*u_long_p;
	uchar_t			*u_char_p;
	char			*char_p;
	vt_trace_t		*trace_p;
	vt_generic_t		*generic_p;
	vt_version_t		*version_p;
	vt_title_t		*title_p;
	vt_label_t		*label_p;
	vt_pagesize_t		*pagesize_p;
	vt_num_cpus_t		*num_cpus_p;
	vt_cpu_t		*cpu_p;
	vt_data_4_t		*data_4_p;
	vt_data_8_t		*data_8_p;
	vt_data_16_t		*data_16_p;
	vt_data_32_t		*data_32_p;
	vt_data_64_t		*data_64_p;
	vt_abs_time_t		*abs_time_p;
	vt_start_time_t		*start_time_p;
	vt_elapsed_time_t	*elapsed_time_p;
	vt_total_time_t		*total_time_p;
	vt_kthread_id_t		*kthread_id_p;
	vt_uthread_id_t		*uthread_id_p;
	vt_clock_frequency_t	*clock_frequency_p;
	vt_raw_kthread_id_t	*raw_kthread_id_p;
	vt_raw_uthread_id_t	*raw_uthread_id_p;
	vt_kthread_label_t	*kthread_label_p;
	vt_uthread_label_t	*uthread_label_p;
	vt_process_name_t	*process_name_p;
	vt_process_fork_t	*process_fork_p;
} vt_pointer_t;

/*
 * Data structures used by vtrace(VTR_INFO)
 */

typedef struct vt_global_info {
	hrtime_t	elapsed_time;
	int	v_major;
	int	v_minor;
	int	v_micro;
	int	tracing_state;
	int	tracing_pid;
	int	ncpus;
	void	*tracedata_ptrs;
} vt_global_info_t;

typedef struct vt_cpu_info {
	ulong_t	cpu_online;
	ulong_t	reserved;
	u_longlong_t	bytes_flushed;
	ulong_t	max_flushsize;
	ulong_t	tbuf_size;
	ulong_t	tbuf_lowater;
	char	**tbuf_headp;
	char	*tbuf_start;
	char	*tbuf_end;
	char	*tbuf_wrap;
	char	*tbuf_head;
	char	*tbuf_tail;
	char	*tbuf_redzone;
	char	*tbuf_overflow;
	uchar_t	*real_event_map;
	uchar_t	*event_map;
	struct file *trace_file;
} vt_cpu_info_t;

#endif	/* _ASM */

/*
 * Requests for the vtrace() system call.
 */

#define	VTR_INIT		0	/* initialize trace buffers */
#define	VTR_FILE		1	/* bind user fd to CPU trace buffer */
#define	VTR_EVENTMAP		2	/* set entire event_map */
#define	VTR_EVENT		3	/* enable a single event */
#define	VTR_START		4	/* start tracing (up to pause state) */
#define	VTR_PAUSE		5	/* momentarily pause (stop) tracing */
#define	VTR_RESUME		6	/* resume tracing from a pause */
#define	VTR_INFO		7	/* return tracing status info */
#define	VTR_FLUSH		8	/* flush trace buffers to files */
#define	VTR_RESET		9	/* stop tracing, free resources */
#define	VTR_TEST		10	/* run tracing speed tests */
#define	VTR_PROCESS		11	/* trace only selected processes */
#define	VTR_GET_STRING		12	/* copy out string from kaddr */

/*
 * Flags for vtrace() requests.
 */

#define	VTR_INFO_GLOBAL		0	/* return global info */
#define	VTR_INFO_PERCPU		1	/* return per-CPU info */
#define	VTR_NOFORCE		0	/* don't force the vtrace request */
#define	VTR_FORCE		1	/* force the vtrace request */

/*
 * Tracing states.
 */

#define	VTR_STATE_NULL		0x01	/* tracing subsystem inactive */
#define	VTR_STATE_READY		0x02	/* ready to begin tracing */
#define	VTR_STATE_PAUSE		0x04	/* tracing paused, but fac 0 still on */
#define	VTR_STATE_ACTIVE	0x08	/* tracing active */
#define	VTR_STATE_PERPROC	0x10	/* tracing active on per-proc basis */
#define	VTR_STATE_HALTED	0x20	/* tracing halted */

/*
 * Per-process tracing modes.
 */

#define	VTR_PROCESS_NULL	0x01	/* not doing per-process tracing */
#define	VTR_PROCESS_TRACE	0x02	/* trace procs in process_list */
#define	VTR_PROCESS_NOTRACE	0x04	/* trace procs not in process_list */

#define	MAX_TRACE_NPROCS	64	/* max procs traced in PERPROC mode */

#ifdef	_KERNEL
#ifdef	TRACE

#ifndef _ASM

extern uchar_t	null_event_map[];
extern ulong_t	bytes2data[];
extern void	trace_resume(void);
extern void	trace_pause(void);
extern void	trace_halt(void);
extern void	trace_reset(int);
extern int	trace_flush(int);
extern int	tracing_state;
extern int	trace_set_process_list(int, pid_t *, int);
extern void	trace_check_process(void);
extern void	inittrace(void);
extern void	trace_0(ulong_t);
extern void	trace_1(ulong_t, ulong_t);
extern void	trace_2(ulong_t, ulong_t, ulong_t);
extern void	trace_3(ulong_t, ulong_t, ulong_t, ulong_t);
extern void	trace_4(ulong_t, ulong_t, ulong_t, ulong_t, ulong_t);
extern void	trace_5(ulong_t, ulong_t, ulong_t, ulong_t, ulong_t, ulong_t);
extern void	trace_write_buffer(ulong_t *, ulong_t);
extern uchar_t	trace_label(uchar_t, uchar_t, ushort_t, char *, int);
extern void	trace_kthread_label(kthread_id_t, int);
extern void	trace_process_name(ulong_t, char *);
extern void	trace_process_fork(ulong_t, ulong_t);
extern hrtime_t	get_vtrace_time(void);
extern uint_t	get_vtrace_frequency(void);

#endif	/* _ASM */

#define	TRACE_N(fac, tag, name, len, func) \
	{ \
		uchar_t xvt_info = \
			CPU->cpu_trace.event_map[FT2EVENT(fac, tag)]; \
		if (xvt_info & VT_ENABLED) { \
			if (!(xvt_info & VT_USED)) \
				xvt_info = trace_label(fac, tag, len, \
					name, -1); \
			func; \
		} \
	}

#else	/* TRACE */

#define	TRACE_N(fac, tag, name, len, func)

#endif	/* TRACE */
#endif	/* _KERNEL */

#define	TRACE_0(fac, tag, name) \
	TRACE_N(fac, tag, name, 4, \
	trace_0(FTT2HEAD(fac, tag, xvt_info)))

#define	TRACE_1(fac, tag, name, d1) \
	TRACE_N(fac, tag, name, 8, \
	trace_1(FTT2HEAD(fac, tag, xvt_info), (ulong_t)(d1)))

#define	TRACE_2(fac, tag, name, d1, d2) \
	TRACE_N(fac, tag, name, 12, \
	trace_2(FTT2HEAD(fac, tag, xvt_info), (ulong_t)(d1), (ulong_t)(d2)))

#define	TRACE_3(fac, tag, name, d1, d2, d3) \
	TRACE_N(fac, tag, name, 16, \
	trace_3(FTT2HEAD(fac, tag, xvt_info), (ulong_t)(d1), (ulong_t)(d2), \
		(ulong_t)(d3)))

#define	TRACE_4(fac, tag, name, d1, d2, d3, d4) \
	TRACE_N(fac, tag, name, 20, \
	trace_4(FTT2HEAD(fac, tag, xvt_info), (ulong_t)(d1), (ulong_t)(d2), \
		(ulong_t)(d3), (ulong_t)(d4)))

#define	TRACE_5(fac, tag, name, d1, d2, d3, d4, d5) \
	TRACE_N(fac, tag, name, 24, \
	trace_5(FTT2HEAD(fac, tag, xvt_info), (ulong_t)(d1), (ulong_t)(d2), \
		(ulong_t)(d3), (ulong_t)(d4), (ulong_t)(d5)))

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VTRACE_H */
