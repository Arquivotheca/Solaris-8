/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _DR_SUBR_H
#define	_DR_SUBR_H

#pragma ident	"@(#)dr_subr.h	1.50	99/04/22 SMI"

#include <dr_daemon.h>
#include <dr_kernel.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * These definitions determine which special IO support is included.
 */
#define	DR_NEW_SONOMA		/* The new Sonoma hot_add script */
/* #define SONOMA */		/* The old Sonoma support (unsupported) */
#define	DR_NF_FDDI		/* the 'nf' SBUS-based FDDI device */
#define	DR_PF_FDDI		/* the 'pf' PCI-based FDDI device */
/* #define DR_BF_PBF_FDDI */	/* the 'bf' and 'pbf' FDDI devices */
/* #define DR_OLDS */		/* OnLine DiskSuite */

/*
 * This macro tests if a board is in range
 */
#define	BOARD_IN_RANGE(board)	(board >= 0 && board < MAX_BOARDS)

#ifndef NODAEMON
/*
 * If compiling a dummy daemon, we don't need to know
 * about the daemon internal routines, but we do need the defines
 * for TEST_CRASH_DAEMON below.
 */
#include <sys/dr.h>
#include <sys/sfdr.h>

#ifdef NO_DRAGON
#define	NODRKERN
#endif NO_DRAGON

#ifndef MIN
#define	MIN(a, b)	(((a) < (b)) ? (a) : (b))
#endif !MIN

/* For exec_command */
#define	MAX_CMD_LINE 64

#ifdef	_XFIRE
/* For detachable check */
#define	DR_MEM_FLOOR	512		/* Min domain memory */
#endif	/* _XFIRE */

/*
 * Global Variables
 */
extern dr_err_t dr_err;
extern time_t	drain_start_time;

/*
 * Command line flags
 */
extern int	verbose;		/* verbose info logging */
extern int	noapdaemon;		/* don't talk to ap daemon */
extern int	no_smtd_kill;		/* don't kill off smtd daemon */
extern int	no_net_unplumb;		/* don't unplumb nets for detach */

#ifdef DR_TEST_VECTOR
/*
 * Globals and routines associated with testing failed DR operations
 */
extern int dr_fail_op;		/* fail op every dr_fail_op times */
extern char dr_test_vector_msg[];

extern void dr_test_vector_clear(void);
extern void dr_test_vector_set(dr_board_state_t op);

#endif DR_TEST_VECTOR
/*
 * Global Routines
 */
extern sfdr_state_t get_dr_state(int board);
extern sfdr_stat_t *get_dr_status(int board);
extern int set_dr_state(int board, dr_board_state_t state);
extern int dr_issue_ioctl(int targ, int brd, int cmd, void *argp, int testmask);
extern void dr_signal_operation_complete(int board);
extern void dr_signal_operation_in_progress(int board);
extern void update_cpu_info_detach(processorid_t cpuid);
extern void autoconfig(void);
extern int exec_command(char *path, char *cmdline[MAX_CMD_LINE]);
extern int dosys(const char *cmd);
extern void dr_ap_init(void);
extern void dr_ap_notify(int board, dr_board_state_t state);
extern void get_io_info(int board, attached_board_infop_t bcp);
extern int dr_unplumb_network(int board);
extern void dr_restart_net_daemons(void);
extern int dr_get_cpu0(void);
extern int dr_get_partn_cpus(int cpuid, int *partition, int *numcpus,
			int syscpu[MAX_CPU_UNITS_PER_BOARD * MAX_BOARDS]);

extern dr_board_state_t dr_get_board_state(int board);
extern void dr_init_attach(brd_init_attach_t *attachp);
extern void dr_complete_attach(int board);
extern void dr_abort_attach_board(int board);
extern void dr_detachable_board(int board);
extern void dr_drain_board_resources(int board);
extern void dr_detach_board(brd_detach_t *bdp, detach_errp_t dep);
extern void dr_finish_detach(void);
extern int dr_get_mib_update_pending(void);
extern void dr_init_sysmap(void);
extern void dr_finish_detach(void);
extern void dr_abort_detach_board(int board);
extern void dr_unsafe_devices(unsafe_devp_t udp);
extern void dr_get_attached_board_info(brd_info_t *bip,
					attached_board_infop_t bcp);
extern board_mem_configp_t get_mem_config(int board, board_mem_configp_t mcp);
extern void dr_get_obp_board_configuration(int board, board_configp_t brdcfgp);
extern void free_mem_config(board_mem_configp_t mp);
extern void free_cpu_config(board_cpu_configp_t cp);
extern int board_in_use(int board);
extern void cant_abort_complete_instead(int board);
extern int dr_stop_rsm_daemons(int board);
extern void dr_restart_rsm_daemons(void);
extern void dr_rsm_hot_add(void);
extern void dr_new_sonoma(void);
#ifdef _XFIRE
extern void dr_get_sysbrd_info(sysbrd_infop_t brd_info);
#endif /* _XFIRE */

extern void dr_logerr(int error, int errno, char *msg);
/* PRINTFLIKE1 */
extern void dr_loginfo(char *format, ...);
extern void dr_report_errors(sfdr_ioctl_arg_t *ioctl_arg, int errno, char *msg);
extern int valid_dr_auth(struct svc_req *req);
extern char *drv_alias2nm(char *alias);
extern void hash_drv_aliases();

#endif NODAEMON

/*
 * Unit Test Scaffolding.
 * Enabled if DR_TEST_CONFIG is defined.
 */

/*
 * These defines, if present in dr_testioctl, will cause
 * the indicated ioctl to fail.  Edit dr_subr.c if these defines are modified
 * or added to.
 */
#define	DR_IOCTL_PROBE		0x00000001
#define	DR_IOCTL_IATTACH	0x00000002
#define	DR_IOCTL_CATTACH	0x00000004
#define	DR_IOCTL_CHECKDET	0x00000008
#define	DR_IOCTL_DEVHOLD	0x00000010
#define	DR_IOCTL_DEVDETACH	0x00000020
#define	DR_IOCTL_DEVRESUME	0x00000040
#define	DR_IOCTL_MVCPU0		0x00000080
#define	DR_IOCTL_GETSTATE	0x00000100
#define	DR_IOCTL_SETSTATE	0x00000200
#define	DR_IOCTL_SAFDEV		0x00000400
#define	DR_IOCTL_MEMCONFIG	0x00000800
#define	DR_IOCTL_MEMCOST	0x00001000
#define	DR_IOCTL_CPUCONFIG	0x00002000
#define	DR_IOCTL_CPUCOST	0x00004000
#define	DR_IOCTL_MEMSTATE	0x00008000
#ifdef _XFIRE
#define	DR_IOCTL_BRDINFO	0x00010000
#endif /* _XFIRE */
#define	DR_IOCTL_GETCPU0	0x00010001

#ifdef DR_TEST_CONFIG
/*
 * For testing, this word contains a bitmask of those
 * ioctls or operations we should cause to fail.
 */
extern void dr_test_control(drmsg_t string, test_statusp_t resultp);
extern int dr_testioctl;
extern int dr_testconfig;
extern int dr_testerrno;
extern void set_test_config(char *line);

#ifdef TEST_SVC

/*
 * If we're a single application, our test crash macro
 * expands to a setjmp back to the test driver menu.
 */
#include <setjmp.h>

extern jmp_buf test_setjmp_buf;

#define	TEST_CRASH_DAEMON(mask) \
	if (dr_testconfig & (mask)) longjmp(test_setjmp_buf, mask);

#else TEST_SVC
/*
 * Macro which can be used to crash the deamon if
 * requested via dr_testconfig flag.  Used mostly
 * to check affect of RPC calls timing out and our ability
 * to restart from these crashes.
 */
#define	TEST_CRASH_DAEMON(mask) \
	if (dr_testconfig & (mask)) exit(1);

#endif TEST_SVC

/*
 * Bit masks used by TEST_CRASH_DAEMON.  Set in dr_testconfig.
 */
#define	DR_RPC_STATE		0x00000001	/* get_board_state */
#define	DR_RPC_IATTACH		0x00000002	/* initiate_attach */
#define	DR_RPC_CATTACH		0x00000004	/* complete_attach */
#define	DR_RPC_AATTACH		0x00000008	/* abort_attach */
#define	DR_RPC_FATTACH		0x00000010	/* attach_finished */
#define	DR_RPC_DBOARD		0x00000020	/* detachable_board */
#define	DR_RPC_DRAIN		0x00000040	/* drain_board_resources */
#define	DR_RPC_LOCK		0x00000080	/* lock_board_resources */
#define	DR_RPC_DETACH		0x00000100	/* detach_board */
#define	DR_RPC_ADETACH		0x00000200	/* abort_detach_board */
#define	DR_RPC_FDETACH		0x00000400	/* detach_finished */
#define	DR_RPC_CPU0MV		0x00000800	/* cpu0_moved */
#define	DR_RPC_OBPCONFIG	0x00001000	/* get_obp_board_config */
#define	DR_RPC_BRDINFO		0x00002000	/* get_board_info */
#define	DR_RPC_GETCPU0		0x00004000	/* get_cpu0 */
#define	DR_RPC_UNSAFED		0x00008000	/* unsafe_devices */

/* these control crashing prior to updating the state */
#define	DR_CRASH_ATTACH_INIT		0x00010000
#define	DR_CRASH_COMPLETE_ATTACH_IP	0x00020000
#define	DR_CRASH_OS_ATTACHED		0x00040000
#define	DR_CRASH_DRAIN			0x00080000
#define	DR_CRASH_LOCK			0x00100000
#define	DR_CRASH_DETACH_IP_2		0x00200000
#define	DR_CRASH_OS_DETACHED		0x00400000
#define	DR_CRASH_BEFORE_PROBE		0x00800000

#else
/*
 * When we're not testing, this macro is a nop
 */
#define	TEST_CRASH_DAEMON(mask)

#endif DR_TEST_CONFIG

#ifdef	__cplusplus
}
#endif

#endif /* _DR_SUBR_H */
