/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * RPCGEN module for DR Daemon-DR Application interface. The DR daemon
 * implements the host side interface to the system board attach/detach 
 * functions of Dynamic Reconfiguration (DR) for SuperDragon. 
 *
 */

%/*
% * Copyright (c) 1998 by Sun Microsystems, Inc.
% * All rights reserved.
% */
%
%#pragma	ident	"@(#)dr_daemon.x	1.38	98/08/12 SMI"

#ifdef RPC_HDR

#ifdef _XFIRE
%#include <sys/dr.h>
%#include <sys/sfdr.h>
%#undef	MAX_BOARDS
%#define	MAX_BOARDS			(16)
%#undef	MAX_CPU_UNITS_PER_BOARD 
%#define	MAX_CPU_UNITS_PER_BOARD	(4)
%#undef	MAX_SBUS_SLOTS_PER_IOC
%#define	MAX_SBUS_SLOTS_PER_IOC		(2)
#endif /* _XFIRE */

%#define	DR_MAGIC	0x44524452
%#define	DR_USER	"ssp"
%#define	DR_HOST_APDX	"-ssp"
#endif

const MAXLEN = 256;
const MAXMSGLEN = 128;

typedef string name_t<MAXLEN>;
typedef string drmsg_t<MAXMSGLEN>;

/*
 * Board states shared between the DR application and daemon.
 * Note the states which are marked as daemon only states.  The
 * DR application will not see these states.
 */
enum dr_board_state_t {
	DR_ERROR_STATE = -1,	/* state cannot be determined */

	DR_NULL_STATE = 0,	/* daemon only state - initial board */
				/*    state (MUST be 0). */

	DR_NO_BOARD,		/* daemon: no kernel usage of the board */
				/* appl: kernel not using _and_ */
				/*     board not present */

	DR_PRESENT,		/* appl: kernel not using _and_ */
				/*     board present */
#ifdef _XFIRE

	DR_NO_DOMAIN,
#endif /* _XFIRE */

	DR_ATTACH_INIT,		/* INITIATE_ATTACH_BOARD done */

	DR_OS_ATTACHED,		/* COMPLETE_ATTACH_BOARD done */
				/* appl needs to finish the attach */

	DR_IN_USE,		/* board in use by kernel */

	DR_DRAIN,		/* DRAIN_BOARD_RESOURCES done */

	DR_CPU0_MOVED,		/* LOCK_BOARD_RESOURCES done, but appl */
				/* must update cpu0 information on ssp */

	DR_OS_DETACHED,		/* DETACH_BOARD done */
				/* appl needs to finish the detach */
	
	DR_BOARD_WEDGED,	/* Error during attach/detach operation. */
				/* Board in unrecoverable, unknown state */

	DR_INIT_ATTACH_IP,	/* daemon internal -- init attach in prog. */
	DR_COMPLETE_ATTACH_IP,	/* daemon internal -- cmpl attach in prog. */
	DR_DRAIN_IP,		/* daemon internal -- drain in progress */
	DR_DETACH_IP,		/* daemon internal -- detach in progress */
	DR_DETACH_IP_2,		/* daemon internal -- detach in progress */

	DR_LOOPBACK,		/* libdr internal -- loopback board */

	DR_NUM_STATES
};

/*-------------------------------------	dr_err_t */

typedef struct dr_err_t *dr_errp_t;

struct dr_err_t {
	int		err;		/* non-zero if an error */
	int		errno;		/* system errno code, if applicable */
	drmsg_t		msg;		/* text explaining error */
	dr_board_state_t state;		/* state of board after RPC call */
};

/*
 * If err != 0, the msg field should be displayed by the application 
 * since it details the cause of the error.
 *
 * All error returns from RPC calls are simple success/fail returns which
 * can be checked for as 0 (DR_SUCCESS) and non-zero (fail).  
 */
const DRV_SUCCESS =		0;		/* all RPC calls */
const DRV_FAIL =		1;

/*------------------------------------- brd_init_attach_t */

struct brd_init_attach_t {
	int	board_slot;	/* slot number of board to attach */
	int	cpu_on_board;	/* CPU ID (0 - 63) from post. */
};

/*------------------------------------- brd_detach_t */

struct brd_detach_t {
	int	board_slot;	/* slot number of board to detach */
	int	force_flag;	/* true if we should force the detach quiesce*/
};

/*------------------------------------- brd_info_t */

struct brd_info_t {
	int		board_slot;	/* slot number of board */
	unsigned int	flag;		/* what units info request is for */
};

/*
 * Values for the flag field.  Information will
 * be retrieved for all device classes requested.
*/
const BRD_CPU =		1;
const BRD_IO =		2;
const BRD_MEM_CONFIG =	4;	/* report memory config information */
const BRD_MEM_COST =	8;	/* report memory detach cost info */
const BRD_MEM_DRAIN =	16;	/* report memory draining info */

%#define BRD_ALL (BRD_CPU|BRD_IO|BRD_MEM_CONFIG|BRD_MEM_COST|BRD_MEM_DRAIN)

/*------------------------------------- unsafe_dev_t */

typedef struct unsafe_dev_t *unsafe_devp_t;

struct unsafe_dev_t {
	dr_err_t	dr_err;
	name_t		unsafe_devs<>;	/* array of unsafe device names */
};

/*------------------------------------- cpu0_status_t */

typedef struct cpu0_status_t *cpu0_statusp_t;

struct cpu0_status_t {
	dr_err_t	dr_err;
	int		cpu0;
};

/*------------------------------------- detach_errp_t */

/*
 * NOTE: the hswperr_t data structure or a pointer to it _cannot_ be
 * used in the RPC interface since hswperr_t contains  unions and
 * RPC alters the structure definition to make them discriminated unions.
 *
 * The daemon gets the hswperr_t structure from the kernel and libdr
 * passes this structure to libssp routines which decode the errors into
 * an ascii string. We must transfer the structure unchanged from host
 * to ssp to accomplish this.
 *
 * Pass this structure as a buffer of characters <length, buffer>.  Libdr
 * will do a simple sanity check that the size of the buffer corresponds to
 * the size of the hswperr_t structure.  If the size of the buffer is zero,
 * there are no hswp errors to display.  Since libdr and the daemon should
 * include the same <hswp/hswp.h>, this should be sufficient.
 */
typedef struct detach_err_t *detach_errp_t;

struct detach_err_t {
	dr_err_t	dr_err;
	unsigned char	dr_hswperr_buff<>;
#ifdef _XFIRE
	int		dr_board;	/* source board */
	int		dr_rename;	/* 0x1 means yes, 0x0 means no*/
	int		dr_tboard;	/* target board */
	u_int		dr_saddr;	/* upper 32 bits of orig
					   source board address */
	u_int		dr_taddr;	/* original target board addr */
#endif /* _XFIRE */
};

/*------------------------------------- test_status_t */

typedef struct test_status_t *test_statusp_t;

struct test_status_t {
	dr_err_t	dr_err;
	drmsg_t		display;	/* display to see results of op */
};


/*
 * The following structures describe the board configuration
 * after the INITIATE_ATTACH_BOARD request has completed.  The 
 * information is derived from the OBP structures and returned via
 * GET_OBP_BOARD_CONFIGURATION.
 *
 * Once a board is completely attached, it's configuration can be
 * retrieved via GET_BOARD_CONFIGURATION.
 */

/*-------------------------------------	obp_cpu_config_t */

struct obp_cpu_config_t {
	int		cpuid;
	int		frequency;	/* in MHz */
#ifdef _XFIRE
	float		ecache_size;	/* in MBytes */
#endif /* _XFIRE */
#ifdef _XFIRE
	int		mask;
#endif /* _XFIRE */
};

/*-------------------------------------	sbus_config_t */

typedef struct sbus_config_t *sbus_configp_t;

struct sbus_config_t {
	name_t		name;
	sbus_configp_t	next;
};

#ifdef _XFIRE
/*------------------------------------- mem_config_t */

struct mem_config_t {
        u_int           l_pfn;	/* lowest page frame number in chunk */
        u_int           h_pfn;	/* highest page frame number in chunk */
};
#endif /* _XFIRE */

/*------------------------------------- board_mem_config_t */

/*
 * If mem_if is zero, interleave and mqh info is
 * not valid and should not be printed.
 */
typedef struct board_mem_config_t *board_mem_configp_t;

struct board_mem_config_t {
	unsigned int	sys_mem;		/* ambient system MBytes */
	unsigned int	dr_min_mem;		/* in MBytes */
	unsigned int	dr_max_mem;		/* in MBytes */
	drmsg_t		dr_mem_detach;		/* meaning of dr-mem-detach */
	int		mem_size;		/* mem on board in MBytes */
	int		mem_if;			/* interleave factor */
#ifdef _XFIRE
        unsigned        interleave_board;       /* boards interleaved with
                                                   this one */
        mem_config_t    mem_pages;		/* page_frame_number
                                                   range present on this 
                                                   board */
	unsigned int	perm_memory;		/* 0 = no perm mem on brd,
						   1 = perm mem on brd */
#endif /* _XFIRE */
};

/*-------------------------------------	board_config_t */

typedef struct board_config_t *board_configp_t;

struct board_config_t {
	dr_err_t	dr_err;
	int		board_slot;

	/*
	 * cpu-unit
	 */
	int		cpu_cnt;		/* number of cpus present */
	obp_cpu_config_t	cpu[MAX_CPU_UNITS_PER_BOARD];

	/*
	 * mem-unit
	 */
	board_mem_config_t mem;

	/*
	 * sbus controllers
	 */
#ifdef _XFIRE
	sbus_configp_t	ioc0[MAX_SBUS_SLOTS_PER_IOC];
	sbus_configp_t	ioc1[MAX_SBUS_SLOTS_PER_IOC];
#endif /* _XFIRE */
};

/*
 * The following structures describe the configuration and cost info
 * of a board  which is in use by the kernel.  This is retrieved via the
 * GET_ATTACHED_BOARD_INFO request.
 */

			/* CPU INFO */

/*-------------------------------------	proclist_t */


struct proclist_t {
	int		pid;
/****					keep it simple for now
	int		uid;
	name_t		cmd;
****/
};

/*-------------------------------------	cpu_info_t */


struct cpu_info_t {
	int		cpuid;		/* 0-63 */
	drmsg_t		cpu_state;	/* ascii string describing cpu state */
	int		partition;	/* 0-63: cpu partition # */
	int		partn_size;	/* # of cpu's in the partition */
					/* including this one */
	int		num_user_threads_bound;	
	int		num_sys_threads_bound;
	proclist_t	proclist<>;
};

/*-------------------------------------	board_cpu_config_t */

typedef struct board_cpu_config_t *board_cpu_configp_t;

struct board_cpu_config_t {
	int		cpu_cnt;		/* number of cpus present */
	int		cpu0;			/* cpuid of the bootproc */
	cpu_info_t cpu[MAX_CPU_UNITS_PER_BOARD];
};

			/* SBUS DEVICE INFO */

/*-------------------------------------	sbus_usage_t */

/*
 * There may be multiple usage structures per device since a device
 * may be subdivided (such as in disk partitions) or may be known by
 * different names (such as the tape drive name variations).  If a
 * device is known via an alias such as introduced by the AP
 * subsystem, these alias names are reported here.
 *
 * The opt_info field is optional text describing how the device
 * is used.  For example:
 *
 *	use					opt_info string
 *	---					---------------
 *	mount point				/export/home
 *	disk partition used as swap space	swap		
 *	interface status			up (xx.xx.xx.xx)
 */

typedef struct sbus_usage_t *sbus_usagep_t;

struct sbus_usage_t {
	name_t		name;		/* name such as /dev path */
	name_t		opt_info;	/* optional, see above */
	int		usage_count;	/* -1 if not available, otherwise 
					   valid count */
	sbus_usagep_t	next;
};

/*------------------------------------- sbus_device_t */

typedef struct sbus_device_t *sbus_devicep_t;

struct sbus_device_t {
	name_t		name;		/* eg sd0 */
	sbus_usagep_t	usage;		/* uses of the device */
	sbus_devicep_t	next;		/* next device for the controller */
};

/*------------------------------------- ap_info_t */

struct ap_info_t {
	int	is_alternate;		/* ctlr is part of AP database */
	int	is_active;		/* ctlr is active AP ctlr */
};

/*------------------------------------- sbus_cntrl_t */

/*
 * Many devices have a parent or controller. such as SCSI controllers.
 * For devices such as fddi (bf), there is no controller.  In this
 * case, the name field of this structure will be NULL, and the device
 * information will contain all relevant information.
 */
typedef struct sbus_cntrl_t *sbus_cntrlp_t;

struct sbus_cntrl_t {
	name_t 		name;		/* eg: isp0, qec0 or NULL */
	ap_info_t	ap_info;
	sbus_devicep_t	devices;	/* devices attached to this cntrller */
	sbus_cntrlp_t	next;		/* next controller in the slot */
};

			/* MEMORY INFO */

/*-------------------------------------	board_mem_cost_t */

/*
 * This structure tells us the actual or estimated cost of
 * detaching this board.  The cost consists of the amount of
 * memory which will be lost due to the detach operation.
 */
typedef struct board_mem_cost_t *board_mem_costp_t;

struct board_mem_cost_t {
	int		actualcost;	   /* see MEMCOST_* consts below */
	int		mem_pshrink;	   /* pages lost/gained due to mem 
					      reconfig */
	unsigned int	mem_pdetach;	   /* pages lost which are resident 
					      on the board */
	int		mem_flush;	   /* reserved for future */
};

/*
 * Defines for the value of the actualcost field above
 */
const MEMCOST_UNAVAIL = -1;
const MEMCOST_ESTIMATE = 0;
const MEMCOST_ACTUAL = 1;
	
/*-------------------------------------	board_mem_cost_t */

/*
 * This structure tells us the status of the in-progress memory
 * drain operation.  This informatin should only be retrieved once the
 * memory drain (hold) operation has been started.  Either check for
 * (board_mem_cost_t.actualcost == 1) or confirm the state is DR_DRAIN.
 * When drain is not in progress, this structure just contains zeros.
 */
typedef struct board_mem_drain_t *board_mem_drainp_t;

struct board_mem_drain_t {
	int			mem_drain_percent; /* percent of drain done */
	unsigned int		mem_kb_left;	   /* number of KB remaining */
	long			mem_drain_start;   /* time_t from time(2)) */
	long			mem_current_time;  /* time_t from time(2)) */
};

/*-------------------------------------	attached_board_info_t */

/*
 * Note that the information returned in this structure is controlled
 * by the user supplied flag field.  The user may select which
 * information is desired at this time.
 */
typedef struct attached_board_info_t *attached_board_infop_t;

struct attached_board_info_t {
	dr_err_t	dr_err;
	int		board_slot;		/* board this config is for */
	int		flag;			/* flags config called with */

	/*
	 * cpu-unit
	 */
	board_cpu_configp_t	cpu;		/* returned if BRD_CPU */

	/*
	 * mem-unit
	 */
	board_mem_configp_t	mem_config;	/* returned if BRD_MEM_CONFIG*/
	board_mem_costp_t	mem_cost;	/* returned if BRD_MEM_COST */
	board_mem_drainp_t	mem_drain;	/* returned if BRD_MEM_DRAIN */

	/*
	 * io-unit
	 *
	 * Returned if BRD_IO
	 *
	 * sbus controllers and attached devices
	 * Each sbus slot may have a linked list of controllers in that
	 * slot (multiple controllers per sbus board).  A null pointer means
	 * the slot is empty.
	 */
#ifdef _XFIRE
	sbus_cntrlp_t	ioc0[MAX_SBUS_SLOTS_PER_IOC];	
	sbus_cntrlp_t	ioc1[MAX_SBUS_SLOTS_PER_IOC];	
#endif /* _XFIRE */

};

#ifdef _XFIRE
typedef struct sysbrd_info_t *sysbrd_infop_t;

struct sysbrd_info_t {
	dr_err_t	dr_err;
	int	brd_addr[MAX_BOARDS];
};
#endif /* _XFIRE */


/*-------------------------------------	RPC stuff */

/*
 * In the RPC calls below.  A single int
 * input argument is the system board number
 */
program DRPROG {
	version DRVERS {

		dr_errp_t
			GET_BOARD_STATE( int ) = 1;

		/*
		 * System Board Attach functions.
		 */
		dr_errp_t
			INITIATE_ATTACH_BOARD( brd_init_attach_t ) = 2;
		dr_errp_t
			ABORT_ATTACH_BOARD( int ) = 3;
		dr_errp_t
			COMPLETE_ATTACH_BOARD( int ) = 4;
		dr_errp_t
			ATTACH_FINISHED( int ) = 5;

		/*
		 * System Board Detach functions.
		 */
		dr_errp_t
			DETACHABLE_BOARD( int ) = 6;
		dr_errp_t
			DRAIN_BOARD_RESOURCES( int ) = 7;
		detach_errp_t
			DETACH_BOARD( brd_detach_t ) = 9;
		dr_errp_t
			ABORT_DETACH_BOARD( int ) = 10;
		dr_errp_t
			CPU0_MOVE_FINISHED( int ) = 11;
		dr_errp_t
			DETACH_FINISHED( int ) = 12;

		/*
		 * Query routines:
		 */
		board_configp_t
			GET_OBP_BOARD_CONFIGURATION( int ) = 13;
		attached_board_infop_t
			GET_ATTACHED_BOARD_INFO( brd_info_t ) = 14;
		unsafe_devp_t
			UNSAFE_DEVICES( void ) = 15;
		cpu0_statusp_t
			GET_CPU0( void ) = 16;
#ifdef _XFIRE
		sysbrd_infop_t
			GET_SYSBRD_INFO( void ) = 19;
#endif /* _XFIRE */

		/*
		 * run autoconfig at the users request
		 */
		dr_errp_t
			RUN_AUTOCONFIG( void ) = 17;

		/*
		 * Test control routine which allows user to
		 * force certain error conditions.  Only enabled
		 * if daemon compiled with DR_TEST_CONFIG defined.
		 */
		test_statusp_t
			TEST_CONTROL( drmsg_t ) = 18;
#ifdef	_XFIRE
	} = 4;
#endif
} = 300326;	/* official DR daemon RPC program number (from Sun) */
