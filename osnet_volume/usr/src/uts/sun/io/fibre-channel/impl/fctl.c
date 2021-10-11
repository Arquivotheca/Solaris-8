/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Fibre channel Transport Library (fctl)
 *
 * Function naming conventions:
 *		Functions called from ULPs begin with fc_ulp_
 *		Functions called from FCAs begin with fc_fca_
 *		Internal functions begin with fctl_
 *
 * Fibre channel packet layout:
 *        +---------------------+<--------+
 *        |                     |         |
 *        | ULP Packet private  |         |
 *        |                     |         |
 *        +---------------------+         |
 *        |                     |---------+
 *        |  struct  fc_packet  |---------+
 *        |                     |         |
 *        +---------------------+<--------+
 *        |                     |
 *        | FCA Packet private  |
 *        |                     |
 *        +---------------------+
 *
 * So you  loved  the  ascii  art ?  It's  strongly  desirable  to  cache
 * allocate the entire packet in one common  place.  So we define a set a
 * of rules.  In a  contiguous  block of memory,  the top  portion of the
 * block points to ulp packet  private  area, next follows the  fc_packet
 * structure used  extensively by all the consumers and what follows this
 * is the FCA packet private.  Note that given a packet  structure, it is
 * possible  to get to the  ULP  and  FCA  Packet  private  fields  using
 * ulp_private and fca_private fields (which hold pointers) respectively.
 *
 * It should be noted with a grain of salt that ULP Packet  private  size
 * varies  between two different  ULP types, So this poses a challenge to
 * compute the correct  size of the whole block on a per port basis.  The
 * transport  layer  doesn't have a problem in dealing with  FCA   packet
 * private  sizes as it is the sole  manager of ports  underneath.  Since
 * it's not a good idea to cache allocate  different  sizes of memory for
 * different ULPs and have the ability to choose from one of these caches
 * based on ULP type during every packet  allocation,  the transport some
 * what  wisely (?)  hands off this job of cache  allocation  to the ULPs
 * themselves.
 *
 * That means FCAs need to make their  packet  private size  known to the
 * transport   to  pass  it  up  to  the   ULPs.  This  is  done   during
 * fc_fca_attach().  And the transport passes this size up to ULPs during
 * fc_ulp_port_attach() of each ULP.
 *
 * This  leaves  us with  another  possible  question;  How  are  packets
 * allocated for ELS's started by the transport  itself ?  Well, the port
 * driver  during  attach  time, cache  allocates  on a per port basis to
 * handle ELSs too.
 */
#pragma ident	"@(#)fctl.c	1.5	99/10/25 SMI"

#include <sys/note.h>
#include <sys/types.h>
#include <sys/varargs.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/modctl.h>
#include <sys/open.h>
#include <sys/kmem.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/promif.h>
#include <sys/fibre-channel/fc.h>
#include <sys/fibre-channel/impl/fc_ulpif.h>
#include <sys/fibre-channel/impl/fc_portif.h>
#include <sys/fibre-channel/impl/fc_fcaif.h>
#include <sys/fibre-channel/impl/fctl_private.h>

int nwwn_table_size = NWWN_HASH_TABLE_SIZE;
int did_table_size = D_ID_HASH_TABLE_SIZE;
int pwwn_table_size = PWWN_HASH_TABLE_SIZE;

static fc_ulp_module_t *fctl_ulp_modules;
static fc_fca_port_t *fctl_fca_portlist;
static fc_nwwn_list_t *nwwn_hash_table;
static int fctl_node_count;

static char fctl_greeting[] =
	"About %s ULP module (type = %x):\n"
	"\tThere already exists one or more ULP module of the\n"
	"\tsame FC-4 type.  Left in this state, unpredictable\n"
	"\tresults  could  occur if  these  ULP  modules  are\n"
	"\tintended  to be used  for the  same  functionality\n"
	"\t(for eg, a SCSI initiator)\n";

static char *fctl_undefined = "Undefined";

static krwlock_t fctl_ulp_lock;
static krwlock_t fctl_port_lock;
static kmutex_t nwwn_hash_mutex;

#if	!defined(lint)
_NOTE(MUTEX_PROTECTS_DATA(nwwn_hash_mutex, nwwn_hash_table))
_NOTE(MUTEX_PROTECTS_DATA(nwwn_hash_mutex, fctl_node_count))
_NOTE(RWLOCK_PROTECTS_DATA(fctl_ulp_lock, ulp_module::mod_next
    ulp_module::mod_ports ulp_ports::port_handle))
_NOTE(DATA_READABLE_WITHOUT_LOCK(ulp_module::mod_info))
_NOTE(MUTEX_PROTECTS_DATA(ulp_ports::port_mutex, ulp_ports::port_statec
    ulp_ports::port_dstate))
#endif /* lint */

extern struct mod_ops mod_miscops;

static struct modlmisc modlmisc = {
	&mod_miscops,		/* type of module */
	"FC Transport Library v1.5 "
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

static struct bus_ops fctl_fca_busops = {
	BUSO_REV,
	nullbusmap,			/* bus_map */
	NULL,				/* bus_get_intrspec */
	NULL,				/* bus_add_intrspec */
	NULL,				/* bus_remove_intrspec */
	i_ddi_map_fault,		/* bus_map_fault */
	ddi_dma_map,			/* bus_dma_map */
	ddi_dma_allochdl,		/* bus_dma_allochdl */
	ddi_dma_freehdl,		/* bus_dma_freehdl */
	ddi_dma_bindhdl,		/* bus_dma_bindhdl */
	ddi_dma_unbindhdl,		/* bus_unbindhdl */
	ddi_dma_flush,			/* bus_dma_flush */
	ddi_dma_win,			/* bus_dma_win */
	ddi_dma_mctl,			/* bus_dma_ctl */
	fctl_fca_bus_ctl,		/* bus_ctl */
	ddi_bus_prop_op,		/* bus_prop_op */
};

struct kmem_cache *fctl_job_cache;

static fc_errmap_t fc_errlist [] = {
	{ FC_FAILURE, 		"Operation failed" },
	{ FC_SUCCESS, 		"Operation success" },
	{ FC_CAP_ERROR, 	"Capability error" },
	{ FC_CAP_FOUND, 	"Capability found" },
	{ FC_CAP_SETTABLE, 	"Capability settable" },
	{ FC_UNBOUND, 		"Port not bound" },
	{ FC_NOMEM, 		"No memory" },
	{ FC_BADPACKET, 	"Bad packet" },
	{ FC_OFFLINE, 		"Port offline" },
	{ FC_OLDPORT, 		"Old Port" },
	{ FC_NO_MAP, 		"No map available" },
	{ FC_TRANSPORT_ERROR, 	"Transport error" },
	{ FC_ELS_FREJECT, 	"ELS Frejected" },
	{ FC_ELS_PREJECT, 	"ELS PRejected" },
	{ FC_ELS_BAD, 		"Bad ELS request" },
	{ FC_ELS_MALFORMED, 	"Malformed ELS request" },
	{ FC_TOOMANY, 		"Too many commands" },
	{ FC_UB_BADTOKEN, 	"Bad Unsolicited buffer token" },
	{ FC_UB_ERROR, 		"Unsolicited buffer error" },
	{ FC_UB_BUSY, 		"Unsolicited buffer busy" },
	{ FC_BADULP, 		"Bad ULP" },
	{ FC_BADTYPE, 		"Bad Type" },
	{ FC_UNCLAIMED, 	"Not Claimed" },
	{ FC_ULP_SAMEMODULE, 	"Same ULP Module" },
	{ FC_ULP_SAMETYPE, 	"Same ULP Type" },
	{ FC_ABORTED, 		"Command Aborted" },
	{ FC_ABORT_FAILED, 	"Abort Failed" },
	{ FC_BADEXCHANGE, 	"Bad Exchange" },
	{ FC_BADWWN, 		"Bad World Wide Name" },
	{ FC_BADDEV, 		"Bad Device" },
	{ FC_BADCMD, 		"Bad Command" },
	{ FC_BADOBJECT, 	"Bad Object" },
	{ FC_BADPORT, 		"Bad Port" },
	{ FC_NOTTHISPORT, 	"Not on this Port" },
	{ FC_PREJECT, 		"Operation Prejected" },
	{ FC_FREJECT, 		"Operation Frejected" },
	{ FC_PBUSY, 		"Operation Pbusyed" },
	{ FC_FBUSY, 		"Operation Fbusyed" },
	{ FC_ALREADY, 		"Already done" },
	{ FC_LOGINREQ, 		"PLOGI Required" },
	{ FC_RESETFAIL, 	"Reset operation failed" },
	{ FC_INVALID_REQUEST, 	"Invalid Request" },
	{ FC_OUTOFBOUNDS, 	"Out of Bounds" },
	{ FC_TRAN_BUSY, 	"Command transport Busy" },
	{ FC_STATEC_BUSY, 	"State change Busy" },
	{ FC_DEVICE_BUSY,	"Port driver is working on this device" }
};

fc_pkt_reason_t remote_stop_reasons [] = {
	{ FC_REASON_ABTS,	"Abort Sequence"	},
	{ FC_REASON_ABTX,	"Abort Exchange"	},
	{ FC_REASON_INVALID,	NULL			}
};

fc_pkt_reason_t general_reasons [] = {
	{ FC_REASON_HW_ERROR,		"Hardware Error" 		},
	{ FC_REASON_SEQ_TIMEOUT,	"Sequence Timeout"		},
	{ FC_REASON_ABORTED,		"Aborted"			},
	{ FC_REASON_ABORT_FAILED,	"Abort Failed"			},
	{ FC_REASON_NO_CONNECTION,	"No Connection"			},
	{ FC_REASON_XCHG_DROPPED,	"Exchange Dropped"		},
	{ FC_REASON_ILLEGAL_FRAME,	"Illegal Frame"			},
	{ FC_REASON_ILLEGAL_LENGTH,	"Illegal Length"		},
	{ FC_REASON_UNSUPPORTED,	"Unsuported"			},
	{ FC_REASON_RX_BUF_TIMEOUT,	"Receive Buffer Timeout"	},
	{ FC_REASON_FCAL_OPN_FAIL,	"FC AL Open Failed"		},
	{ FC_REASON_OVERRUN,		"Over run"			},
	{ FC_REASON_QFULL,		"Queue Full"			},
	{ FC_REASON_ILLEGAL_REQ,	"Illegal Request",		},
	{ FC_REASON_PKT_BUSY,		"Busy"				},
	{ FC_REASON_OFFLINE,		"Offline"			},
	{ FC_REASON_BAD_XID,		"Bad Exchange Id"		},
	{ FC_REASON_XCHG_BSY,		"Exchange Busy"			},
	{ FC_REASON_NOMEM,		"No Memory"			},
	{ FC_REASON_BAD_SID,		"Bad S_ID"			},
	{ FC_REASON_NO_SEQ_INIT,	"No Sequence Initiative"	},
	{ FC_REASON_DIAG_BUSY,		"Diagnostic Busy"		},
	{ FC_REASON_DMA_ERROR,		"DMA Error"			},
	{ FC_REASON_CRC_ERROR,		"CRC Error"			},
	{ FC_REASON_ABORT_TIMEOUT,	"Abort Timeout"			},
	{ FC_REASON_FCA_UNIQUE,		"FCA Unique"			},
	{ FC_REASON_INVALID,		NULL				}
};

fc_pkt_reason_t rjt_reasons [] = {
	{ FC_REASON_INVALID_D_ID,	"Invalid D_ID"			},
	{ FC_REASON_INVALID_S_ID,	"Invalid S_ID"			},
	{ FC_REASON_TEMP_UNAVAILABLE,	"Temporarily Unavailable"	},
	{ FC_REASON_PERM_UNAVAILABLE,	"Permamnently Unavailable"	},
	{ FC_REASON_CLASS_NOT_SUPP,	"Class Not Supported",		},
	{ FC_REASON_DELIMTER_USAGE_ERROR,
					"Delimeter Usage Error"		},
	{ FC_REASON_TYPE_NOT_SUPP,	"Type Not Supported"		},
	{ FC_REASON_INVALID_LINK_CTRL,	"Invalid Link Control"		},
	{ FC_REASON_INVALID_R_CTL,	"Invalid R_CTL"			},
	{ FC_REASON_INVALID_F_CTL,	"Invalid F_CTL"			},
	{ FC_REASON_INVALID_OX_ID,	"Invalid OX_ID"			},
	{ FC_REASON_INVALID_RX_ID,	"Invalid RX_ID"			},
	{ FC_REASON_INVALID_SEQ_ID,	"Invalid Sequence ID"		},
	{ FC_REASON_INVALID_DF_CTL,	"Invalid DF_CTL"		},
	{ FC_REASON_INVALID_SEQ_CNT,	"Invalid Sequence count"	},
	{ FC_REASON_INVALID_PARAM,	"Invalid Parameter"		},
	{ FC_REASON_EXCH_ERROR,		"Exchange Error"		},
	{ FC_REASON_PROTOCOL_ERROR,	"Protocol Error"		},
	{ FC_REASON_INCORRECT_LENGTH,	"Incorrect Length"		},
	{ FC_REASON_UNEXPECTED_ACK,	"Unexpected Ack"		},
	{ FC_REASON_UNEXPECTED_LR,	"Unexpected Link reset" 	},
	{ FC_REASON_LOGIN_REQUIRED,	"Login Required"		},
	{ FC_REASON_EXCESSIVE_SEQS,	"Excessive Sequences"
					" Attempted"			},
	{ FC_REASON_EXCH_UNABLE,	"Exchange incapable"		},
	{ FC_REASON_ESH_NOT_SUPP,	"Expiration Security Header "
					"Not Supported"			},
	{ FC_REASON_NO_FABRIC_PATH,	"No Fabric Path"		},
	{ FC_REASON_VENDOR_UNIQUE,	"Vendor Unique"			},
	{ FC_REASON_INVALID,		NULL				}
};

fc_pkt_reason_t n_port_busy_reasons [] = {
	{ FC_REASON_PHYSICAL_BUSY,		"Physical Busy"		},
	{ FC_REASON_N_PORT_RESOURCE_BSY,	"Resource Busy"		},
	{ FC_REASON_N_PORT_VENDOR_UNIQUE,	"Vendor Unique"		},
	{ FC_REASON_INVALID,			NULL			}
};

fc_pkt_reason_t f_busy_reasons [] = {
	{ FC_REASON_FABRIC_BSY,		"Fabric Busy"			},
	{ FC_REASON_N_PORT_BSY,		"N_Port Busy"			},
	{ FC_REASON_INVALID,		NULL				}
};

fc_pkt_reason_t ls_ba_rjt_reasons [] = {
	{ FC_REASON_INVALID_LA_CODE,	"Invalid Link Application Code"	},
	{ FC_REASON_LOGICAL_ERROR,	"Logical Error"			},
	{ FC_REASON_LOGICAL_BSY,	"Logical Busy"			},
	{ FC_REASON_PROTOCOL_ERROR_RJT,	"Protocol Error Reject"		},
	{ FC_REASON_CMD_UNABLE,		"Unable to Perform Command"	},
	{ FC_REASON_CMD_UNSUPPORTED,	"Unsupported Command"		},
	{ FC_REASON_VU_RJT,		"Vendor Unique"			},
	{ FC_REASON_INVALID,		NULL				}
};

fc_pkt_reason_t fs_rjt_reasons [] = {
	{ FC_REASON_FS_INVALID_CMD,	"Invalid Command"		},
	{ FC_REASON_FS_INVALID_VER,	"Invalid Version"		},
	{ FC_REASON_FS_LOGICAL_ERR,	"Logical Error"			},
	{ FC_REASON_FS_INVALID_IUSIZE,	"Invalid IU Size"		},
	{ FC_REASON_FS_LOGICAL_BUSY,	"Logical Busy"			},
	{ FC_REASON_FS_PROTOCOL_ERR,	"Protocol Error"		},
	{ FC_REASON_FS_CMD_UNABLE,	"Unable to Perform Command"	},
	{ FC_REASON_FS_CMD_UNSUPPORTED,	"Unsupported Command"		},
	{ FC_REASON_FS_VENDOR_UNIQUE,	"Vendor Unique"			},
	{ FC_REASON_INVALID,		NULL				}
};

fc_pkt_action_t	n_port_busy_actions [] = {
	{ FC_ACTION_SEQ_TERM_RETRY,	"Retry terminated Sequence"	},
	{ FC_ACTION_SEQ_ACTIVE_RETRY,	"Retry Active Sequence"		},
	{ FC_REASON_INVALID,		NULL				}
};

fc_pkt_action_t rjt_timeout_actions [] = {
	{ FC_ACTION_RETRYABLE,		"Retryable"			},
	{ FC_ACTION_NON_RETRYABLE,	"Non Retryable"			},
	{ FC_REASON_INVALID,		NULL				}
};

fc_pkt_expln_t ba_rjt_explns [] = {
	{ FC_EXPLN_NONE,		"No Explanation"		},
	{ FC_EXPLN_INVALID_OX_RX_ID,	"Invalid X_ID"			},
	{ FC_EXPLN_SEQ_ABORTED,		"Sequence Aborted"		},
	{ FC_EXPLN_INVALID,		NULL				}
};

fc_pkt_error_t fc_pkt_errlist[] = {
	{
		FC_PKT_SUCCESS,
		"Operation Success",
		NULL,
		NULL,
		NULL
	},

	{	FC_PKT_REMOTE_STOP,
		"Remote Stop",
		remote_stop_reasons,
		NULL,
		NULL
	},

	{
		FC_PKT_LOCAL_RJT,
		"Local Reject",
		general_reasons,
		rjt_timeout_actions,
		NULL
	},
	{
		FC_PKT_NPORT_RJT,
		"N_Port Reject",
		rjt_reasons,
		rjt_timeout_actions,
		NULL
	},
	{
		FC_PKT_FABRIC_RJT,
		"Fabric Reject",
		rjt_reasons,
		rjt_timeout_actions,
		NULL
	},
	{
		FC_PKT_LOCAL_BSY,
		"Local Busy",
		general_reasons,
		NULL,
		NULL,
	},
	{
		FC_PKT_TRAN_BSY,
		"Transport Busy",
		general_reasons,
		NULL,
		NULL,
	},
	{
		FC_PKT_NPORT_BSY,
		"N_Port Busy",
		n_port_busy_reasons,
		n_port_busy_actions,
		NULL
	},
	{
		FC_PKT_FABRIC_BSY,
		"Fabric Busy",
		f_busy_reasons,
		NULL,
		NULL,
	},
	{
		FC_PKT_LS_RJT,
		"Link Service Reject",
		ls_ba_rjt_reasons,
		NULL,
		NULL,
	},
	{
		FC_PKT_BA_RJT,
		"Basic Reject",
		ls_ba_rjt_reasons,
		NULL,
		ba_rjt_explns,
	},
	{
		FC_PKT_TIMEOUT,
		"Timeout",
		general_reasons,
		rjt_timeout_actions,
		NULL
	},
	{
		FC_PKT_FS_RJT,
		"Fabric Switch Reject",
		fs_rjt_reasons,
		NULL,
		NULL
	},
	{
		FC_PKT_TRAN_ERROR,
		"Packet Transport error",
		general_reasons,
		NULL,
		NULL
	},
	{
		FC_PKT_FAILURE,
		"Packet Failure",
		general_reasons,
		NULL,
		NULL
	},
	{
		FC_PKT_PORT_OFFLINE,
		"Port Offline",
		NULL,
		NULL,
		NULL
	},
	{
		FC_PKT_ELS_IN_PROGRESS,
		"ELS is in Progress",
		NULL,
		NULL,
		NULL
	}
};

int
_init()
{
	int rval;

	rw_init(&fctl_ulp_lock, NULL, RW_DRIVER, NULL);
	rw_init(&fctl_port_lock, NULL, RW_DRIVER, NULL);
	mutex_init(&nwwn_hash_mutex, NULL, MUTEX_DRIVER, NULL);

	nwwn_hash_table = (fc_nwwn_list_t *)kmem_zalloc(
	    sizeof (*nwwn_hash_table) * nwwn_table_size, KM_SLEEP);

	fctl_ulp_modules = NULL;
	fctl_fca_portlist = NULL;
	fctl_node_count = 0;

	fctl_job_cache = kmem_cache_create("fctl_cache",
	    sizeof (job_request_t), 8, fctl_cache_constructor,
	    fctl_cache_destructor, NULL, NULL, NULL, 0);

	if (fctl_job_cache == NULL) {
		kmem_free(nwwn_hash_table,
		    sizeof (*nwwn_hash_table) * nwwn_table_size);
		mutex_destroy(&nwwn_hash_mutex);
		rw_destroy(&fctl_port_lock);
		rw_destroy(&fctl_ulp_lock);
		return (ENOMEM);
	}

	if ((rval = mod_install(&modlinkage)) != 0) {
		kmem_cache_destroy(fctl_job_cache);
		kmem_free(nwwn_hash_table,
		    sizeof (*nwwn_hash_table) * nwwn_table_size);
		mutex_destroy(&nwwn_hash_mutex);
		rw_destroy(&fctl_port_lock);
		rw_destroy(&fctl_ulp_lock);
	}

	return (rval);
}


/*
 * The mod_uninstall code doesn't call _fini when
 * there is living dependent module on fctl. So
 * there is no need to be extra careful here ?
 */
int
_fini()
{
	int rval;

	if ((rval = mod_remove(&modlinkage)) != 0) {
		return (rval);
	}

	kmem_cache_destroy(fctl_job_cache);
	kmem_free(nwwn_hash_table, sizeof (*nwwn_hash_table) *
	    nwwn_table_size);
	mutex_destroy(&nwwn_hash_mutex);
	rw_destroy(&fctl_port_lock);
	rw_destroy(&fctl_ulp_lock);

	return (rval);
}


int
_info(struct modinfo *modinfo_p)
{
	return (mod_info(&modlinkage, modinfo_p));
}


/* ARGSUSED */
static int
fctl_cache_constructor(void *buf, void *cdarg, int kmflag)
{
	job_request_t *job = (job_request_t *)buf;

	mutex_init(&job->job_mutex, NULL, MUTEX_DRIVER, NULL);
	sema_init(&job->job_fctl_sema, 0, NULL, SEMA_DEFAULT, NULL);
	sema_init(&job->job_port_sema, 0, NULL, SEMA_DEFAULT, NULL);

	return (0);
}


/* ARGSUSED */
static void
fctl_cache_destructor(void *buf, void *cdarg)
{
	job_request_t *job = (job_request_t *)buf;

	sema_destroy(&job->job_fctl_sema);
	sema_destroy(&job->job_port_sema);
	mutex_destroy(&job->job_mutex);
}


/*
 * fc_ulp_add:
 *		Add a ULP module
 *
 * Return Codes:
 *		FC_ULP_SAMEMODULE
 *		FC_SUCCESS
 *		FC_FAILURE
 *
 *   fc_ulp_add  prints  a warning message if there is  already a
 *   similar ULP type  attached and this is unlikely to change as
 *   we trudge along.  Further, this  function  returns a failure
 *   code if the same  module  attempts to add more than once for
 *   the same FC-4 type.
 */
int
fc_ulp_add(fc_ulp_modinfo_t *ulp_info)
{
	fc_ulp_module_t *mod;
	fc_ulp_module_t *prev;
	job_request_t 	*job;
	fc_fca_port_t 	*fca_port;

	ASSERT(ulp_info != NULL);

	rw_enter(&fctl_ulp_lock, RW_WRITER);

	for (mod = fctl_ulp_modules, prev = NULL; mod; mod = mod->mod_next) {
		ASSERT(mod->mod_info != NULL);

		if (ulp_info == mod->mod_info &&
		    ulp_info->ulp_type == mod->mod_info->ulp_type) {
			rw_exit(&fctl_ulp_lock);
			return (FC_ULP_SAMEMODULE);
		}

		if (ulp_info->ulp_type == mod->mod_info->ulp_type) {
			cmn_err(CE_WARN, fctl_greeting, ulp_info->ulp_name,
			    ulp_info->ulp_type);
		}
		prev = mod;
	}

	mod = kmem_zalloc(sizeof (*mod), KM_SLEEP);
	mod->mod_info = ulp_info;
	mod->mod_next = NULL;

	if (prev) {
		prev->mod_next = mod;
	} else {
		fctl_ulp_modules = mod;
	}

	/*
	 * Schedule a job to each port's job_handler
	 * thread to attach their ports with this ULP.
	 */
	rw_enter(&fctl_port_lock, RW_READER);
	for (fca_port = fctl_fca_portlist; fca_port != NULL;
	    fca_port = fca_port->port_next) {

		job = fctl_alloc_job(JOB_ATTACH_ULP, JOB_TYPE_FCTL_ASYNC,
		    NULL, NULL, KM_SLEEP);

		fctl_enque_job(fca_port->port_handle, job);
	}
	rw_exit(&fctl_port_lock);

	rw_exit(&fctl_ulp_lock);

	return (FC_SUCCESS);
}


/*
 * fc_ulp_remove
 *	Remove a ULP module
 *
 * A misbehaving ULP may call this routine while I/Os are in progress.
 * Currently there is no mechanism to detect it to fail such a request.
 *
 * Return Codes:
 *		FC_SUCCESS
 *		FC_FAILURE
 */
int
fc_ulp_remove(fc_ulp_modinfo_t *ulp_info)
{
	fc_ulp_module_t *mod;
	fc_ulp_module_t *prev;

	rw_enter(&fctl_ulp_lock, RW_WRITER);
	for (mod = fctl_ulp_modules, prev = NULL; mod != NULL;
	    mod = mod->mod_next) {
		if (mod->mod_info == ulp_info) {
			break;
		}
		prev = mod;
	}

	if (mod) {
		if (prev) {
			prev->mod_next = mod->mod_next;
		} else {
			fctl_ulp_modules = mod->mod_next;
		}
		rw_exit(&fctl_ulp_lock);

		kmem_free(mod, sizeof (*mod));

		return (FC_SUCCESS);
	}
	rw_exit(&fctl_ulp_lock);

	return (FC_FAILURE);
}


/*
 * The callers typically cache allocate the packet, complete the
 * DMA setup for pkt_cmd and pkt_resp fields of the packet and
 * call this function to see if the FCA is interested in doing
 * its own intialization. For example, socal may like to initialize
 * the soc_hdr which is pointed to by the pkt_fca_private field
 * and sitting right below fc_packet_t in memory.
 */
int
fc_ulp_init_packet(opaque_t port_handle, fc_packet_t *pkt, int sleep)
{
	int rval;
	fc_port_t *port = (fc_port_t *)port_handle;

	rval = port->fp_fca_tran->fca_init_pkt(port->fp_fca_handle, pkt, sleep);

	return (rval);
}


/*
 * This function is called before destroying the cache allocated
 * fc_packet to free up (and uninitialize) any resource specially
 * allocated by FCAs during tran_init_pkt(). The un_init_pkt
 * vector could be set to NULL
 */
int
fc_ulp_uninit_packet(opaque_t port_handle, fc_packet_t *pkt)
{
	int rval;
	fc_port_t *port = (fc_port_t *)port_handle;

	rval = port->fp_fca_tran->fca_un_init_pkt(port->fp_fca_handle, pkt);

	return (rval);
}


int
fc_ulp_getportmap(opaque_t port_handle, fc_portmap_t *map, uint32_t *len,
    int flag)
{
	int		job_code;
	fc_port_t 	*port;
	job_request_t	*job;

	port = (fc_port_t *)port_handle;

	mutex_enter(&port->fp_mutex);
	if (port->fp_statec_busy) {
		mutex_exit(&port->fp_mutex);
		return (FC_STATEC_BUSY);
	}

	if (FC_PORT_STATE_MASK(port->fp_state) == FC_STATE_OFFLINE) {
		mutex_exit(&port->fp_mutex);
		return (FC_OFFLINE);
	}

	if (port->fp_dev_count && (port->fp_dev_count ==
	    port->fp_total_devices)) {
		mutex_exit(&port->fp_mutex);
		fctl_fillout_map(port, &map, len, 1);
		return (FC_SUCCESS);
	}
	mutex_exit(&port->fp_mutex);

	switch (flag) {
	case FC_ULP_PLOGI_DONTCARE:
		job_code = JOB_PORT_GETMAP;
		break;

	case FC_ULP_PLOGI_PRESERVE:
		job_code = JOB_PORT_GETMAP_PLOGI_ALL;
		break;

	default:
		return (FC_INVALID_REQUEST);
	}
	/*
	 * Submit a job request to the job handler
	 * thread to get the map and wait
	 */
	job = fctl_alloc_job(job_code, 0, NULL, NULL, KM_SLEEP);
	job->job_private = (opaque_t)&map;
	job->job_arg = (opaque_t)len;
	fctl_enque_job(port, job);

	fctl_jobwait(job);
	/*
	 * The result of the last I/O operation is
	 * in job_code. We don't care to look at it
	 * Rather we look at the number of devices
	 * that are found to fill out the map for
	 * ULPs.
	 */
	fctl_dealloc_job(job);

	return (FC_SUCCESS);
}


int
fc_ulp_login(opaque_t port_handle, fc_packet_t **ulp_pkt, uint32_t listlen)
{
	int			rval = FC_SUCCESS;
	int 			job_flags;
	uint32_t		count;
	fc_packet_t		**tmp_array;
	job_request_t 		*job;
	fc_port_t 		*port = (fc_port_t *)port_handle;

	/*
	 * If the port is OFFLINE, or if the port driver is
	 * being SUSPENDED/PM_SUSPENDED/DETACHED, block all
	 * PLOGI operations
	 */
	mutex_enter(&port->fp_mutex);
	if (port->fp_statec_busy) {
		mutex_exit(&port->fp_mutex);
		return (FC_STATEC_BUSY);
	}

	if (FC_PORT_STATE_MASK(port->fp_state) ==
	    FC_STATE_OFFLINE || FP_CANT_ALLOW_ELS(port)) {
		mutex_exit(&port->fp_mutex);
		return (FC_OFFLINE);
	}
	mutex_exit(&port->fp_mutex);

	tmp_array = kmem_zalloc(sizeof (*tmp_array) * listlen, KM_SLEEP);
	for (count = 0; count < listlen; count++) {
		tmp_array[count] = ulp_pkt[count];
	}

	job_flags = ((ulp_pkt[0]->pkt_tran_flags) & FC_TRAN_NO_INTR)
	    ? 0 : JOB_TYPE_FCTL_ASYNC;

#ifdef	DEBUG
	{
		int next;
		int count;
		int polled;

		polled = ((ulp_pkt[0]->pkt_tran_flags) &
		    FC_TRAN_NO_INTR) ? 0 : JOB_TYPE_FCTL_ASYNC;

		for (count = 0; count < listlen; count++) {
			next = ((ulp_pkt[count]->pkt_tran_flags)
			    & FC_TRAN_NO_INTR) ? 0 : JOB_TYPE_FCTL_ASYNC;
			ASSERT(next == polled);
		}
	}
#endif	/* DEBUG */

	job = fctl_alloc_job(JOB_PLOGI_GROUP, job_flags, NULL, NULL, KM_SLEEP);
	job->job_ulp_pkts = tmp_array;
	job->job_ulp_listlen = listlen;

	while (listlen--) {
		fc_packet_t *pkt;

		pkt = tmp_array[listlen];
		if (pkt->pkt_pd != NULL) {
			mutex_enter(&pkt->pkt_pd->pd_mutex);
			if (pkt->pkt_pd->pd_flags == PD_ELS_IN_PROGRESS) {
				/*
				 * Set the packet state and let the port
				 * driver call the completion routine
				 * from its thread
				 */
				pkt->pkt_state = FC_PKT_ELS_IN_PROGRESS;
			} if (pkt->pkt_pd->pd_state == PORT_DEVICE_INVALID ||
			    pkt->pkt_pd->pd_type == PORT_DEVICE_OLD) {
				pkt->pkt_state = FC_PKT_LOCAL_RJT;
				pkt->pkt_pd = NULL;
			} else {
				pkt->pkt_state = FC_PKT_SUCCESS;
				pkt->pkt_pd->pd_flags = PD_ELS_IN_PROGRESS;
			}
			mutex_exit(&pkt->pkt_pd->pd_mutex);
		} else {
			pkt->pkt_state = FC_PKT_SUCCESS;
		}
	}
	fctl_enque_job(port, job);

	if (!(job_flags & JOB_TYPE_FCTL_ASYNC)) {
		fctl_jobwait(job);
		rval = job->job_result;
		fctl_dealloc_job(job);
	}

	return (rval);
}


/*
 * For unspecified reasons, fc_ulp_logout is designed
 * to be no-callback function (unlike fc_ulp_login).
 * So sit around until the LOGO completes.
 */
int
fc_ulp_logout(fc_port_device_t *pd)
{
	int		rval;
	fc_port_t	*port;
	job_request_t 	*job;

	mutex_enter(&pd->pd_mutex);
	if ((port = pd->pd_port) == NULL) {
		mutex_exit(&pd->pd_mutex);
		return (FC_BADPORT);
	}

	if (pd->pd_count <= 0 ||
	    pd->pd_state != PORT_DEVICE_LOGGED_IN) {
		mutex_exit(&pd->pd_mutex);
		return (FC_LOGINREQ);
	}

	pd->pd_count--;
	if (pd->pd_count) {
		mutex_exit(&pd->pd_mutex);
		return (FC_SUCCESS);
	}
	mutex_exit(&pd->pd_mutex);

	mutex_enter(&port->fp_mutex);
	if (port->fp_statec_busy) {
		mutex_exit(&port->fp_mutex);
		mutex_enter(&pd->pd_mutex);
		pd->pd_count++;
		mutex_exit(&pd->pd_mutex);
		return (FC_STATEC_BUSY);
	}

	if (FC_PORT_STATE_MASK(port->fp_state) ==
	    FC_STATE_OFFLINE || FP_CANT_ALLOW_ELS(port)) {
		mutex_exit(&port->fp_mutex);
		mutex_enter(&pd->pd_mutex);
		pd->pd_count++;
		mutex_exit(&pd->pd_mutex);
		return (FC_OFFLINE);
	}
	mutex_exit(&port->fp_mutex);

	mutex_enter(&pd->pd_mutex);
	pd->pd_flags = PD_ELS_IN_PROGRESS;
	mutex_exit(&pd->pd_mutex);

	job = fctl_alloc_job(JOB_LOGO_ONE, 0, NULL, NULL, KM_SLEEP);
	ASSERT(job != NULL);
	job->job_ulp_pkts = (void *)pd;
	FCTL_SET_JOB_COUNTER(job, 1);

	fctl_enque_job(port, job);
	fctl_jobwait(job);
	rval = job->job_result;
	fctl_dealloc_job(job);

	return (rval);
}


int
fc_ulp_getmap(la_wwn_t *wwn_list, uint32_t *listlen)
{
	int 		index;
	fc_device_t	*node;
	fc_nwwn_elem_t 	*head;

	mutex_enter(&nwwn_hash_mutex);
	if (fctl_node_count == 0) {
		*listlen = fctl_node_count;
		mutex_exit(&nwwn_hash_mutex);
		return (FC_NO_MAP);
	}

	if (*listlen < fctl_node_count) {
		*listlen = fctl_node_count;
		mutex_exit(&nwwn_hash_mutex);
		return (FC_FAILURE);
	}

	for (index = 0; index < nwwn_table_size; index++) {
		head = nwwn_hash_table[index].hash_head;
		while (head != NULL) {
			node = head->fc_device;
			mutex_enter(&node->fd_mutex);
			*wwn_list++ = node->fd_node_name;
			mutex_exit(&node->fd_mutex);
			head = head->hash_next;
		}
	}
	mutex_exit(&nwwn_hash_mutex);

	return (FC_SUCCESS);
}


fc_device_t *
fc_ulp_get_device_by_nwwn(la_wwn_t *nwwn, int *error)
{
	fc_device_t *node;

	node = fctl_get_device_by_nwwn(nwwn);
	*error = (node == NULL) ? FC_FAILURE : FC_SUCCESS;

	return (node);
}


fc_port_device_t *
fc_ulp_get_port_device(opaque_t port_handle, la_wwn_t *pwwn, int *error,
    int create)
{
	fc_port_t 		*port;
	job_request_t		*job;
	fc_port_device_t 	*pd;

	port = (fc_port_t *)port_handle;
	pd = fctl_get_port_device_by_pwwn(port, pwwn);

	if (pd != NULL) {
		*error = FC_SUCCESS;
		return (pd);
	}

	mutex_enter(&port->fp_mutex);
	if (FC_IS_TOP_SWITCH(port->fp_topology) && create) {
		uint32_t	d_id;
		fctl_ns_req_t 	*ns_cmd;

		mutex_exit(&port->fp_mutex);

		job = fctl_alloc_job(JOB_NS_CMD, 0, NULL, NULL, KM_SLEEP);

		if (job == NULL) {
			*error = FC_NOMEM;
			return (pd);
		}

		ns_cmd = fctl_alloc_ns_cmd(sizeof (ns_req_gid_pn_t),
		    sizeof (ns_resp_gid_pn_t), sizeof (ns_resp_gid_pn_t),
		    0, KM_SLEEP);

		if (ns_cmd == NULL) {
			*error = FC_NOMEM;
			return (pd);
		}
		ns_cmd->ns_cmd_code = NS_GID_PN;
		FCTL_NS_GID_PN_INIT(ns_cmd->ns_cmd_buf, pwwn);

		job->job_result = FC_SUCCESS;
		job->job_private = (void *)ns_cmd;
		FCTL_SET_JOB_COUNTER(job, 1);
		fctl_enque_job(port, job);
		fctl_jobwait(job);

		if (job->job_result != FC_SUCCESS) {
			*error = job->job_result;
			fctl_free_ns_cmd(ns_cmd);
			fctl_dealloc_job(job);
			return (pd);
		}
		d_id = ((ns_resp_gid_pn_t *)ns_cmd->ns_data_buf)->NS_PID;
		fctl_free_ns_cmd(ns_cmd);

		ns_cmd = fctl_alloc_ns_cmd(sizeof (ns_req_gan_t),
		    sizeof (ns_resp_gan_t), 0, FCTL_NS_CREATE_DEVICE,
		    KM_SLEEP);
		ASSERT(ns_cmd != NULL);

		ns_cmd->ns_gan_max = 1;
		ns_cmd->ns_cmd_code = NS_GA_NXT;
		ns_cmd->ns_gan_sid = FCTL_GAN_START_ID;
		FCTL_NS_GAN_INIT(ns_cmd->ns_cmd_buf, d_id - 1);
		job->job_result = FC_SUCCESS;
		job->job_private = (void *)ns_cmd;
		FCTL_SET_JOB_COUNTER(job, 1);
		fctl_enque_job(port, job);
		fctl_jobwait(job);

		fctl_free_ns_cmd(ns_cmd);
		if (job->job_result != FC_SUCCESS) {
			*error = job->job_result;
			fctl_dealloc_job(job);
			return (pd);
		}
		fctl_dealloc_job(job);

		/*
		 * Check if the port device is created now.
		 */
		pd = fctl_get_port_device_by_pwwn(port, pwwn);
		*error = (pd == NULL) ? FC_FAILURE : FC_SUCCESS;
	} else {
		mutex_exit(&port->fp_mutex);
		*error = FC_FAILURE;
	}

	return (pd);
}


int
fc_ulp_get_portlist(fc_device_t *device, opaque_t *portlist, uint32_t *len)
{
	int 			count;
	fc_port_device_t	*pd;

	mutex_enter(&device->fd_mutex);
	ASSERT(device->fd_ports != NULL);

	for (count = 0, pd = device->fd_ports; pd != NULL; count++) {
		pd = pd->pd_port_next;
	}
	mutex_exit(&device->fd_mutex);

	if (*len < count) {
		*len = count;
		return (FC_NOMEM);
	}

	mutex_enter(&device->fd_mutex);
	for (pd = device->fd_ports; pd != NULL; pd = pd->pd_port_next) {
		mutex_enter(&pd->pd_mutex);
		*portlist++ = (opaque_t)pd->pd_port;
		mutex_exit(&pd->pd_mutex);
	}
	mutex_exit(&device->fd_mutex);
	return (FC_SUCCESS);
}


/* ARGSUSED */
int
fc_ulp_node_ns(fc_device_t *node, uint32_t cmd, void *object)
{
	/*
	 * No need for this function; All NS object
	 * operations can be performed using
	 * fc_ulp_port_ns()
	 */
	return (FC_FAILURE);
}


/*
 * If a NS object exists in the host and query is performed
 * on that object, we should retrieve it from our basket
 * and return it right here, there by saving a request going
 * all the up to the Name Server.
 */
int
fc_ulp_port_ns(opaque_t port_handle, fc_port_device_t *pd, fc_ns_cmd_t *ns_req)
{
	int 		rval;
	int		fabric;
	job_request_t	*job;
	fctl_ns_req_t	*ns_cmd;
	fc_port_t	*port = (fc_port_t *)port_handle;

	mutex_enter(&port->fp_mutex);
	fabric = FC_IS_TOP_SWITCH(port->fp_topology) ? 1 : 0;
	mutex_exit(&port->fp_mutex);

	/*
	 * Name server query can't be performed for devices not in Fabric
	 */
	if (!fabric && pd) {
		return (FC_BADOBJECT);
	}

	if (FC_IS_CMD_A_REG(ns_req->ns_cmd)) {
		if (pd == NULL) {
			rval = fctl_update_host_ns_values(port, ns_req);
			if (rval != FC_SUCCESS) {
				return (rval);
			}
		} else {
			/*
			 * Guess what, FC-GS-2 currently prohibits (not
			 * in the strongest language though) setting of
			 * NS object values by other ports. But we might
			 * get that changed to at least accommodate setting
			 * symbolic node/port names - But if disks/tapes
			 * were going to provide a method to set these
			 * values directly (which in turn might register
			 * with the NS when they come up; yep, for that
			 * to happen the disks will have to be very well
			 * behaved Fabric citizen) we won't need to
			 * register the symbolic port/node names for
			 * other ports too (rather send down SCSI commands
			 * to the devices to set the names)
			 *
			 * Be that as it may, let's continue to fail
			 * registration requests for other ports. period.
			 */
			return (FC_BADOBJECT);
		}

		if (!fabric) {
			return (FC_SUCCESS);
		}
	} else if (!fabric) {
		return (fctl_retrieve_host_ns_values(port, ns_req));
	}

	job = fctl_alloc_job(JOB_NS_CMD, 0, NULL, NULL, KM_SLEEP);
	ASSERT(job != NULL);

	ns_cmd = fctl_alloc_ns_cmd(ns_req->ns_req_len,
	    ns_req->ns_resp_len, ns_req->ns_resp_len, 0, KM_SLEEP);
	ASSERT(ns_cmd != NULL);
	ns_cmd->ns_cmd_code = ns_req->ns_cmd;
	bcopy(ns_req->ns_req_payload, ns_cmd->ns_cmd_buf,
	    ns_req->ns_req_len);

	job->job_private = (void *)ns_cmd;
	fctl_enque_job(port, job);
	fctl_jobwait(job);
	rval = job->job_result;

	if (ns_req->ns_resp_len >= ns_cmd->ns_data_len) {
		bcopy(ns_cmd->ns_data_buf, ns_req->ns_resp_payload,
		    ns_cmd->ns_data_len);
	}
	bcopy(&ns_cmd->ns_resp_hdr, &ns_req->ns_resp_hdr,
	    sizeof (fc_ct_header_t));

	fctl_free_ns_cmd(ns_cmd);
	fctl_dealloc_job(job);

	return (rval);
}


int
fc_ulp_transport(opaque_t port_handle, fc_packet_t *pkt)
{
	int			rval;
	fc_port_t 		*port;
	fc_port_device_t	*pd;

	port = (fc_port_t *)port_handle;

	/*
	 * If the port is OFFLINE, or if the port driver is
	 * being SUSPENDED/PM_SUSPENDED/RESUMED/PM_RESUMED/
	 * DETACHED, block all I/O operations on this port.
	 */
	mutex_enter(&port->fp_mutex);
	if (port->fp_statec_busy) {
		mutex_exit(&port->fp_mutex);
		return (FC_STATEC_BUSY);
	}
	if ((FC_PORT_STATE_MASK(port->fp_state)) ==
	    FC_STATE_OFFLINE || FP_CANT_ALLOW_TRANSPORT(port)) {
		mutex_exit(&port->fp_mutex);
		return (FC_OFFLINE);
	}
	mutex_exit(&port->fp_mutex);

	pd = pkt->pkt_pd;
	if (pd == NULL) {
		return (FC_BADPACKET);
	}
	mutex_enter(&pd->pd_mutex);
	if (pd->pd_state != PORT_DEVICE_LOGGED_IN) {
		rval = (pd->pd_state == PORT_DEVICE_VALID) ?
		    FC_LOGINREQ : FC_BADDEV;
		mutex_exit(&pd->pd_mutex);
		return (rval);
	}

	if (pd->pd_type == PORT_DEVICE_OLD ||
	    pd->pd_state == PORT_DEVICE_INVALID) {
		mutex_exit(&pd->pd_mutex);
		return (FC_BADDEV);
	}

	if (pd->pd_flags != PD_IDLE) {
		mutex_exit(&pd->pd_mutex);
		return (FC_DEVICE_BUSY);
	}
	mutex_exit(&pd->pd_mutex);

	return (port->fp_fca_tran->fca_transport(port->fp_fca_handle, pkt));
}


int
fc_ulp_issue_els(opaque_t port_handle, fc_packet_t *pkt)
{
	fc_port_t 		*port = (fc_port_t *)port_handle;
	fc_port_device_t	*pd;

	/*
	 * If the port is OFFLINE, or if the port driver is
	 * being SUSPENDED/PM_SUSPENDED/DETACHED, block all
	 * ELS operations
	 */
	mutex_enter(&port->fp_mutex);
	if (FC_PORT_STATE_MASK(port->fp_state) ==
	    FC_STATE_OFFLINE || FP_CANT_ALLOW_ELS(port)) {
		mutex_exit(&port->fp_mutex);
		return (FC_OFFLINE);
	}
	if (port->fp_statec_busy) {
		mutex_exit(&port->fp_mutex);
		return (FC_STATEC_BUSY);
	}
	mutex_exit(&port->fp_mutex);

	if ((pd = pkt->pkt_pd) != NULL) {
		mutex_enter(&pd->pd_mutex);
		if (pd->pd_type == PORT_DEVICE_OLD ||
		    pd->pd_state == PORT_DEVICE_INVALID) {
			mutex_exit(&pd->pd_mutex);
			return (FC_BADDEV);
		}
		if (pd->pd_flags != PD_IDLE) {
			mutex_exit(&pd->pd_mutex);
			return (FC_DEVICE_BUSY);
		}
		mutex_exit(&pd->pd_mutex);
	}

	return (port->fp_fca_tran->fca_els_send(port->fp_fca_handle, pkt));
}


int
fc_ulp_uballoc(opaque_t port_handle, uint32_t *count, uint32_t size,
    uint32_t type, uint64_t *tokens)
{
	fc_port_t *port = (fc_port_t *)port_handle;

	return (port->fp_fca_tran->fca_ub_alloc(port->fp_fca_handle,
	    tokens, size, count, type));
}


int
fc_ulp_ubfree(opaque_t port_handle, uint32_t count, uint64_t *tokens)
{
	fc_port_t *port = (fc_port_t *)port_handle;

	return (port->fp_fca_tran->fca_ub_free(port->fp_fca_handle,
	    count, tokens));
}


int
fc_ulp_ubrelease(opaque_t port_handle, uint32_t count, uint64_t *tokens)
{
	fc_port_t *port = (fc_port_t *)port_handle;

	return (port->fp_fca_tran->fca_ub_release(port->fp_fca_handle,
	    count, tokens));
}


int
fc_ulp_abort(opaque_t port_handle, fc_packet_t *pkt, int flags)
{
	fc_port_t *port = (fc_port_t *)port_handle;

	return (port->fp_fca_tran->fca_abort(port->fp_fca_handle, pkt, flags));
}


/*
 * Submit an asynchronous request to the job handler if the sleep
 * flag is set to KM_NOSLEEP, as such calls could have been made
 * in interrupt contexts, and the goal is to avoid busy waiting,
 * blocking on a conditional variable, a semaphore or any of the
 * synchronization primitives. A noticeable draw back with this
 * asynchronous request is that an FC_SUCCESS is returned long
 * before the reset is complete (successful or not).
 */
int
fc_ulp_linkreset(opaque_t port_handle, la_wwn_t *pwwn, int sleep)
{
	int		rval;
	fc_port_t 	*port;
	job_request_t	*job;

	port = (fc_port_t *)port_handle;
	/*
	 * Many a times, this function is called from interrupt
	 * contexts and there have been several dead locks and
	 * hangs - One of the simplest work arounds is to fib
	 * if a RESET is in progress.
	 */
	mutex_enter(&port->fp_mutex);
	if (port->fp_soft_state & FP_SOFT_IN_LINK_RESET) {
		mutex_exit(&port->fp_mutex);
		return (FC_SUCCESS);
	}

	/*
	 * Ward off this reset if a state change is in progress.
	 */
	if (port->fp_statec_busy) {
		mutex_exit(&port->fp_mutex);
		return (FC_STATEC_BUSY);
	}
	port->fp_soft_state |= FP_SOFT_IN_LINK_RESET;
	mutex_exit(&port->fp_mutex);

	if (sleep == KM_SLEEP) {
		job = fctl_alloc_job(JOB_LINK_RESET, 0, NULL, NULL, sleep);
		ASSERT(job != NULL);

		job->job_private = (void *)pwwn;
		FCTL_SET_JOB_COUNTER(job, 1);
		fctl_enque_job(port, job);

		fctl_jobwait(job);

		mutex_enter(&port->fp_mutex);
		port->fp_soft_state &= ~FP_SOFT_IN_LINK_RESET;
		mutex_exit(&port->fp_mutex);

		rval = job->job_result;
		fctl_dealloc_job(job);
	} else {
		job = fctl_alloc_job(JOB_LINK_RESET, JOB_TYPE_FCTL_ASYNC,
		    fctl_link_reset_done, port, sleep);
		if (job == NULL) {
			mutex_enter(&port->fp_mutex);
			port->fp_soft_state &= ~FP_SOFT_IN_LINK_RESET;
			mutex_exit(&port->fp_mutex);
			return (FC_NOMEM);
		}
		job->job_private = (void *)pwwn;
		FCTL_SET_JOB_COUNTER(job, 1);
		fctl_priority_enque_job(port, job);
		rval = FC_SUCCESS;
	}

	return (rval);
}


int
fc_ulp_port_reset(opaque_t port_handle, uint32_t cmd)
{
	int		rval = FC_SUCCESS;
	fc_port_t 	*port = (fc_port_t *)port_handle;

	switch (cmd) {
	case FC_RESET_PORT:
		rval = port->fp_fca_tran->fca_reset(
		    port->fp_fca_handle, FC_FCA_LINK_RESET);
		break;

	case FC_RESET_ADAPTER:
		rval = port->fp_fca_tran->fca_reset(
		    port->fp_fca_handle, FC_FCA_RESET);
		break;

	case FC_RESET_DUMP:
		rval = port->fp_fca_tran->fca_reset(
		    port->fp_fca_handle, FC_FCA_CORE);
		break;

	case FC_RESET_CRASH:
		rval = port->fp_fca_tran->fca_reset(
		    port->fp_fca_handle, FC_FCA_RESET_CORE);
		break;

	default:
		rval = FC_FAILURE;
	}

	return (rval);
}


int
fc_ulp_get_did(fc_port_device_t *pd, uint32_t *d_id)
{
	mutex_enter(&pd->pd_mutex);
	*d_id = pd->PD_PORT_ID;
	mutex_exit(&pd->pd_mutex);

	return (FC_SUCCESS);
}


int
fc_ulp_get_pd_state(fc_port_device_t *pd, uint32_t *state)
{
	mutex_enter(&pd->pd_mutex);
	*state = pd->pd_state;
	mutex_exit(&pd->pd_mutex);

	return (FC_SUCCESS);
}


int
fc_ulp_is_fc4_bit_set(uint32_t *map, uchar_t ulp_type)
{
	map += FC4_TYPE_WORD_POS(ulp_type);

	if (*map & (1 << FC4_TYPE_BIT_POS(ulp_type))) {
		return (FC_SUCCESS);
	}

	return (FC_FAILURE);
}


int
fc_ulp_get_port_instance(opaque_t port_handle)
{
	fc_port_t *port = (fc_port_t *)port_handle;

	return (port->fp_instance);
}


int
fc_ulp_error(int fc_errno, char **errmsg)
{
	return (fctl_error(fc_errno, errmsg));
}


int
fc_ulp_pkt_error(fc_packet_t *pkt, char **state, char **reason,
    char **action, char **expln)
{
	return (fctl_pkt_error(pkt, state, reason, action, expln));
}


/*
 * fc_fca_init
 * 		Overload the FCA bus_ops vector in its dev_ops with
 *		fctl_fca_busops to handle all the INITchilds for "sf"
 *		in one common place.
 *
 *		Should be called from FCA _init routine.
 */
void
fc_fca_init(struct dev_ops *fca_devops_p)
{
#ifndef	__lock_lint
	fca_devops_p->devo_bus_ops = &fctl_fca_busops;
#endif	/* __lock_lint */
}


/*
 * fc_fca_attach
 */
int
fc_fca_attach(dev_info_t *fca_dip, fc_fca_tran_t *tran)
{
	ddi_set_driver_private(fca_dip, (caddr_t)tran);
	return (DDI_SUCCESS);
}


/*
 * fc_fca_detach
 */
int
fc_fca_detach(dev_info_t *fca_dip)
{
	ddi_set_driver_private(fca_dip, NULL);
	return (DDI_SUCCESS);
}


/*
 * Check if the frame is a Link response Frame; Handle all cases (P_RJT,
 * F_RJT, P_BSY, F_BSY fall into this category). Check also for some Basic
 * Link Service responses such as BA_RJT and Extended Link Service response
 * such as LS_RJT. If the response is a Link_Data Frame or something that
 * this function doesn't understand return FC_FAILURE; Otherwise, fill out
 * various fields (state, action, reason, expln) from the response gotten
 * in the packet and return FC_SUCCESS.
 */
int
fc_fca_update_errors(fc_packet_t *pkt)
{
	int ret = FC_SUCCESS;

	switch (pkt->pkt_resp_fhdr.r_ctl) {
	case R_CTL_P_RJT:
	{
		uint32_t prjt;

		prjt = pkt->pkt_resp_fhdr.ro;
		pkt->pkt_state = FC_PKT_NPORT_RJT;
		pkt->pkt_action = (prjt & 0xFF000000) >> 24;
		pkt->pkt_reason = (prjt & 0xFF0000) >> 16;
		break;
	}

	case R_CTL_F_RJT:
	{
		uint32_t frjt;

		frjt = pkt->pkt_resp_fhdr.ro;
		pkt->pkt_state = FC_PKT_FABRIC_RJT;
		pkt->pkt_action = (frjt & 0xFF000000) >> 24;
		pkt->pkt_reason = (frjt & 0xFF0000) >> 16;
		break;
	}

	case R_CTL_P_BSY:
	{
		uint32_t pbsy;

		pbsy = pkt->pkt_resp_fhdr.ro;
		pkt->pkt_state = FC_PKT_NPORT_BSY;
		pkt->pkt_action = (pbsy & 0xFF000000) >> 24;
		pkt->pkt_reason = (pbsy & 0xFF0000) >> 16;
		break;
	}

	case R_CTL_F_BSY_LC:
	case R_CTL_F_BSY_DF:
	{
		uchar_t fbsy;

		fbsy = pkt->pkt_resp_fhdr.type;
		pkt->pkt_state = FC_PKT_FABRIC_BSY;
		pkt->pkt_reason = (fbsy & 0xF0) >> 4;
		break;
	}

	case R_CTL_LS_BA_RJT:
	{
		uint32_t brjt;

		brjt = *(uint32_t *)pkt->pkt_resp;
		pkt->pkt_state = FC_PKT_BA_RJT;
		pkt->pkt_reason = (brjt & 0xFF0000) >> 16;
		pkt->pkt_expln = (brjt & 0xFF00) >> 8;
		break;
	}

	case R_CTL_ELS_RSP:
	{
		la_els_rjt_t *lsrjt;

		lsrjt = (la_els_rjt_t *)pkt->pkt_resp;
		if (lsrjt->LS_CODE == LA_ELS_RJT) {
			pkt->pkt_state = FC_PKT_LS_RJT;
			pkt->pkt_reason = lsrjt->reason;
			pkt->pkt_action = lsrjt->action;
			break;
		}
		/* FALLTHROUGH */
	}

	default:
		ret = FC_FAILURE;
		break;
	}

	return (ret);
}


int
fc_fca_error(int fc_errno, char **errmsg)
{
	return (fctl_error(fc_errno, errmsg));
}


int
fc_fca_pkt_error(fc_packet_t *pkt, char **state, char **reason,
    char **action, char **expln)
{
	return (fctl_pkt_error(pkt, state, reason, action, expln));
}


/*
 * FCA driver's intercepted bus control operations.
 */
static int
fctl_fca_bus_ctl(dev_info_t *fca_dip, dev_info_t *rip,
	ddi_ctl_enum_t op, void *arg, void *result)
{
	switch (op) {
	case DDI_CTLOPS_REPORTDEV:
		break;

	case DDI_CTLOPS_IOMIN:
		break;

	case DDI_CTLOPS_INITCHILD:
		return (fctl_initchild(fca_dip, (dev_info_t *)arg));

	case DDI_CTLOPS_UNINITCHILD:
		return (fctl_uninitchild(fca_dip, (dev_info_t *)arg));

	default:
		return (ddi_ctlops(fca_dip, rip, op, arg, result));
	}

	return (DDI_SUCCESS);
}


/*
 * FCAs indicate the maximum number of ports supported in their
 * tran structure. Fail the INITCHILD if the child port number
 * is any greater than the maximum number of ports supported
 * by the FCA.
 */
static int
fctl_initchild(dev_info_t *fca_dip, dev_info_t *port_dip)
{
	int 		rval;
	int 		port_no;
	int 		port_len;
	char 		name[20];
	fc_fca_tran_t 	*tran;

	port_len = sizeof (port_no);

	rval = ddi_prop_op(DDI_DEV_T_ANY, port_dip, PROP_LEN_AND_VAL_BUF,
	    DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP, "port",
	    (caddr_t)&port_no, &port_len);

	if (rval != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	tran = (fc_fca_tran_t *)ddi_get_driver_private(fca_dip);
	ASSERT(tran != NULL);

	if (port_no < 0 || port_no >= tran->fca_numports) {
		return (DDI_FAILURE);
	}
	(void) sprintf((char *)name, "%x,0", port_no);
	ddi_set_name_addr(port_dip, name);

	return (DDI_SUCCESS);
}


/* ARGSUSED */
static int
fctl_uninitchild(dev_info_t *fca_dip, dev_info_t *port_dip)
{
	ddi_set_name_addr(port_dip, NULL);
	return (DDI_SUCCESS);
}


void
fctl_add_port(fc_port_t *port)
{
	fc_fca_port_t *new;

	new = kmem_zalloc(sizeof (*new), KM_SLEEP);

	rw_enter(&fctl_port_lock, RW_WRITER);
	new->port_handle = port;
	new->port_next = fctl_fca_portlist;
	fctl_fca_portlist = new;
	rw_exit(&fctl_port_lock);
}


void
fctl_remove_port(fc_port_t *port)
{
	fc_ulp_module_t 	*mod;
	fc_fca_port_t 		*prev;
	fc_fca_port_t 		*list;
	fc_ulp_ports_t		*ulp_port;

	rw_enter(&fctl_ulp_lock, RW_WRITER);
	for (mod = fctl_ulp_modules; mod; mod = mod->mod_next) {
		ulp_port = fctl_get_ulp_port(mod, port);
		if (ulp_port == NULL) {
			continue;
		}

#ifndef	__lock_lint
		ASSERT((ulp_port->port_dstate & ULP_PORT_ATTACH) == 0);
#endif /* __lock_lint */

		(void) fctl_remove_ulp_port(mod, port);
	}
	rw_exit(&fctl_ulp_lock);

	rw_enter(&fctl_port_lock, RW_WRITER);

	list = fctl_fca_portlist;
	prev = NULL;
	while (list != NULL) {
		if (list->port_handle == port) {
			if (prev == NULL) {
				fctl_fca_portlist = list->port_next;
			} else {
				prev->port_next = list->port_next;
			}
			kmem_free(list, sizeof (*list));
			break;
		}
		prev = list;
		list = list->port_next;
	}
	rw_exit(&fctl_port_lock);
}


void
fctl_attach_ulps(fc_port_t *port, ddi_attach_cmd_t cmd,
    struct modlinkage *linkage)
{
	int			rval;
	uint32_t		s_id;
	uint32_t		state;
	fc_ulp_module_t 	*mod;
	fc_ulp_port_info_t 	info;
	fc_ulp_ports_t		*ulp_port;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	info.port_linkage = linkage;
	info.port_dip = port->fp_port_dip;
	info.port_handle = (opaque_t)port;
	info.port_dma_attr = port->fp_fca_tran->fca_dma_attr;
	info.port_acc_attr = port->fp_fca_tran->fca_acc_attr;
	info.port_fca_pkt_size = port->fp_fca_tran->fca_pkt_size;
	info.port_reset_action = port->fp_reset_action;

	mutex_enter(&port->fp_mutex);
	s_id = port->FP_PORT_ID;

	info.port_state = port->fp_bind_state;

	switch (state = FC_PORT_STATE_MASK(port->fp_state)) {
	case FC_STATE_LOOP:
	case FC_STATE_NAMESERVICE:
		info.port_state &= ~state;
		info.port_state |= FC_STATE_ONLINE;
		break;

	default:
		break;
	}

	info.port_flags = port->fp_topology;
	info.port_pwwn = port->fp_service_params.nport_ww_name;
	info.port_nwwn = port->fp_service_params.node_ww_name;
	mutex_exit(&port->fp_mutex);

	rw_enter(&fctl_ulp_lock, RW_WRITER);
	for (mod = fctl_ulp_modules; mod; mod = mod->mod_next) {
		if ((ulp_port = fctl_get_ulp_port(mod, port)) != NULL) {
			if (fctl_pre_attach(ulp_port, cmd) == FC_FAILURE) {
				continue;
			}
		} else {
			ulp_port = fctl_add_ulp_port(mod, port, KM_SLEEP);
		}

		rval = mod->mod_info->ulp_port_attach(
		    mod->mod_info->ulp_handle, &info, cmd, s_id);

		fctl_post_attach(mod, ulp_port, cmd, rval);
	}
	rw_exit(&fctl_ulp_lock);
}


static int
fctl_pre_attach(fc_ulp_ports_t *ulp_port, ddi_attach_cmd_t cmd)
{
	int rval = FC_SUCCESS;

	mutex_enter(&ulp_port->port_mutex);

	switch (cmd) {
	case DDI_ATTACH:
		if (ulp_port->port_dstate & ULP_PORT_ATTACH) {
			rval = FC_FAILURE;
		}
		break;

	case DDI_RESUME:
		if (!(ulp_port->port_dstate & ULP_PORT_ATTACH) ||
		    !(ulp_port->port_dstate & ULP_PORT_SUSPEND)) {
			rval = FC_FAILURE;
		}
		break;

	case DDI_PM_RESUME:
		ASSERT((ulp_port->port_dstate & ULP_PORT_SUSPEND) == 0);

		if (!(ulp_port->port_dstate & ULP_PORT_ATTACH) ||
		    !(ulp_port->port_dstate & ULP_PORT_PM_SUSPEND)) {
			rval = FC_FAILURE;
		}
		break;
	}

	if (rval == FC_SUCCESS) {
		ulp_port->port_dstate |= ULP_PORT_BUSY;
	}
	mutex_exit(&ulp_port->port_mutex);

	return (rval);
}


static void
fctl_post_attach(fc_ulp_module_t *mod, fc_ulp_ports_t *ulp_port,
    ddi_attach_cmd_t cmd, int rval)
{
	ASSERT(cmd == DDI_ATTACH || cmd == DDI_RESUME || cmd == DDI_PM_RESUME);

	mutex_enter(&ulp_port->port_mutex);
	ulp_port->port_dstate &= ~ULP_PORT_BUSY;

	if (rval != FC_SUCCESS) {
		caddr_t		op;
		fc_port_t 	*port = (fc_port_t *)ulp_port->port_handle;

		mutex_exit(&ulp_port->port_mutex);

		switch (cmd) {
		case DDI_ATTACH:
			op = "attach";
			break;

		case DDI_RESUME:
			op = "resume";
			break;

		case DDI_PM_RESUME:
			op = "PM resume";
			break;
		}

		cmn_err(CE_WARN, "!fctl(%d): %s failed for %s",
		    port->fp_instance, op, mod->mod_info->ulp_name);

		return;
	}

	switch (cmd) {
	case DDI_ATTACH:
		ulp_port->port_dstate |= ULP_PORT_ATTACH;
		break;

	case DDI_RESUME:
		ulp_port->port_dstate &= ~ULP_PORT_SUSPEND;
		break;

	case DDI_PM_RESUME:
		ulp_port->port_dstate &= ~ULP_PORT_PM_SUSPEND;
		break;
	}
	mutex_exit(&ulp_port->port_mutex);
}


int
fctl_detach_ulps(fc_port_t *port, ddi_detach_cmd_t cmd,
    struct modlinkage *linkage)
{
	int			rval = FC_SUCCESS;
	fc_ulp_module_t 	*mod;
	fc_ulp_port_info_t 	info;
	fc_ulp_ports_t		*ulp_port;

	info.port_linkage = linkage;
	info.port_dip = port->fp_port_dip;
	info.port_handle = (opaque_t)port;
	info.port_dma_attr = port->fp_fca_tran->fca_dma_attr;
	info.port_acc_attr = port->fp_fca_tran->fca_acc_attr;
	info.port_fca_pkt_size = port->fp_fca_tran->fca_pkt_size;

	rw_enter(&fctl_ulp_lock, RW_READER);
	for (mod = fctl_ulp_modules; mod; mod = mod->mod_next) {
		if ((ulp_port = fctl_get_ulp_port(mod, port)) == NULL) {
			continue;
		}

		if (fctl_pre_detach(ulp_port, cmd) != FC_SUCCESS) {
			continue;
		}

		rval = mod->mod_info->ulp_port_detach(
		    mod->mod_info->ulp_handle, &info, cmd);

		fctl_post_detach(mod, ulp_port, cmd, rval);

		if (rval != FC_SUCCESS) {
			break;
		}

		mutex_enter(&ulp_port->port_mutex);
		ulp_port->port_statec = FC_ULP_STATEC_DONT_CARE;
		mutex_exit(&ulp_port->port_mutex);
	}
	rw_exit(&fctl_ulp_lock);

	return (rval);
}


static int
fctl_pre_detach(fc_ulp_ports_t *ulp_port, ddi_detach_cmd_t cmd)
{
	int rval = FC_SUCCESS;

	mutex_enter(&ulp_port->port_mutex);

	switch (cmd) {
	case DDI_DETACH:
		if ((ulp_port->port_dstate & ULP_PORT_ATTACH) == 0) {
			rval = FC_FAILURE;
		}
		break;

	case DDI_SUSPEND:
		if (!(ulp_port->port_dstate & ULP_PORT_ATTACH) ||
		    ulp_port->port_dstate & ULP_PORT_SUSPEND) {
			rval = FC_FAILURE;
		}
		break;

	case DDI_PM_SUSPEND:
		if (!(ulp_port->port_dstate & ULP_PORT_ATTACH) ||
		    ulp_port->port_dstate & ULP_PORT_PM_SUSPEND) {
			rval = FC_FAILURE;
		}
		break;
	}

	if (rval == FC_SUCCESS) {
		ulp_port->port_dstate |= ULP_PORT_BUSY;
	}
	mutex_exit(&ulp_port->port_mutex);

	return (rval);
}


static void
fctl_post_detach(fc_ulp_module_t *mod, fc_ulp_ports_t *ulp_port,
    ddi_detach_cmd_t cmd, int rval)
{
	ASSERT(cmd == DDI_DETACH || cmd == DDI_SUSPEND ||
	    cmd == DDI_PM_SUSPEND);

	mutex_enter(&ulp_port->port_mutex);
	ulp_port->port_dstate &= ~ULP_PORT_BUSY;

	if (rval != FC_SUCCESS) {
		caddr_t		op;
		fc_port_t 	*port = (fc_port_t *)ulp_port->port_handle;

		mutex_exit(&ulp_port->port_mutex);

		switch (cmd) {
		case DDI_DETACH:
			op = "detach";
			break;

		case DDI_SUSPEND:
			op = "suspend";
			break;

		case DDI_PM_SUSPEND:
			op = "PM suspend";
			break;
		}

		cmn_err(CE_WARN, "!fctl(%d): %s failed for %s",
		    port->fp_instance, op, mod->mod_info->ulp_name);

		return;
	}

	switch (cmd) {
	case DDI_DETACH:
		ulp_port->port_dstate &= ~ULP_PORT_ATTACH;
		break;

	case DDI_SUSPEND:
		ulp_port->port_dstate |= ULP_PORT_SUSPEND;
		break;

	case DDI_PM_SUSPEND:
		ulp_port->port_dstate |= ULP_PORT_PM_SUSPEND;
		break;
	}
	mutex_exit(&ulp_port->port_mutex);
}


static fc_ulp_ports_t *
fctl_add_ulp_port(fc_ulp_module_t *ulp_module, fc_port_t *port_handle,
    int sleep)
{
	fc_ulp_ports_t *last;
	fc_ulp_ports_t *next;
	fc_ulp_ports_t *new;

	ASSERT(RW_WRITE_HELD(&fctl_ulp_lock));

	last = NULL;
	next = ulp_module->mod_ports;

	while (next != NULL) {
		last = next;
		next = next->port_next;
	}

	new = kmem_zalloc(sizeof (*new), sleep);
	if (new == NULL) {
		return (new);
	}
	mutex_init(&new->port_mutex, NULL, MUTEX_DRIVER, NULL);

	new->port_handle = port_handle;
	if (last == NULL) {
		ulp_module->mod_ports = new;
	} else {
		last->port_next = new;
	}

	return (new);
}


static int
fctl_remove_ulp_port(struct ulp_module *ulp_module, fc_port_t *port_handle)
{
	fc_ulp_ports_t *last;
	fc_ulp_ports_t *next;

	ASSERT(RW_WRITE_HELD(&fctl_ulp_lock));

	last = NULL;
	next = ulp_module->mod_ports;

	while (next != NULL) {
		if (next->port_handle == port_handle) {
			if (next->port_dstate & ULP_PORT_ATTACH) {
				return (FC_FAILURE);
			}
			break;
		}
		last = next;
		next = next->port_next;
	}

	if (next != NULL) {
		if (last == NULL) {
			ulp_module->mod_ports = next->port_next;
		} else {
			last->port_next = next->port_next;
		}
		mutex_destroy(&next->port_mutex);
		kmem_free(next, sizeof (*next));

		return (FC_SUCCESS);
	} else {
		return (FC_FAILURE);
	}
}


static fc_ulp_ports_t *
fctl_get_ulp_port(struct ulp_module *ulp_module, fc_port_t *port_handle)
{
	fc_ulp_ports_t *next;

	ASSERT(RW_LOCK_HELD(&fctl_ulp_lock));

	for (next = ulp_module->mod_ports; next != NULL;
	    next = next->port_next) {
		if (next->port_handle == port_handle) {
			return (next);
		}
	}

	return (NULL);
}


/*
 * Returning a value that isn't equal to ZERO will
 * result in the callback being rescheduled again.
 */
int
fctl_ulp_statec_cb(caddr_t arg)
{
	uint32_t		s_id;
	uint32_t		new_state;
	fc_port_t		*port;
	fc_ulp_ports_t		*ulp_port;
	fc_ulp_module_t 	*mod;
	fc_port_clist_t 	*clist = (fc_port_clist_t *)arg;

	ASSERT(clist != NULL);

	port = (fc_port_t *)clist->clist_port;

	mutex_enter(&port->fp_mutex);
	s_id = port->FP_PORT_ID;
	mutex_exit(&port->fp_mutex);

	switch (clist->clist_state) {
	case FC_STATE_ONLINE:
		new_state = FC_ULP_STATEC_ONLINE;
		mutex_enter(&port->fp_mutex);
		ASSERT(port->fp_statec_busy > 0);
		FC_STATEC_DONE(port);
		mutex_exit(&port->fp_mutex);
		break;

	case FC_STATE_OFFLINE:
		if (clist->clist_len) {
			new_state = FC_ULP_STATEC_OFFLINE_TIMEOUT;
		} else {
			new_state = FC_ULP_STATEC_OFFLINE;
		}
		break;

	default:
		new_state = FC_ULP_STATEC_DONT_CARE;
		break;
	}

#ifdef	DEBUG
	/*
	 * sanity check for presence of OLD devices in the hash lists
	 */
	if (clist->clist_size) {
		int 			count;
		fc_port_device_t	*pd;

		ASSERT(clist->clist_map != NULL);
		for (count = 0; count < clist->clist_len; count++) {
			if (clist->clist_map[count].map_state ==
			    PORT_DEVICE_INVALID) {
				la_wwn_t 	pwwn;
				fc_portid_t 	d_id;

				pd = clist->clist_map[count].map_pd;
				ASSERT(pd != NULL);

				mutex_enter(&pd->pd_mutex);
				pwwn = pd->pd_port_name;
				d_id = pd->pd_port_id;
				mutex_exit(&pd->pd_mutex);

				pd = fctl_get_port_device_by_pwwn(port, &pwwn);
				ASSERT(pd != clist->clist_map[count].map_pd);

				pd = fctl_get_port_device_by_did(port,
				    d_id.port_id);
				ASSERT(pd != clist->clist_map[count].map_pd);
			}
		}
	}
#endif /* DEBUG */

	rw_enter(&fctl_ulp_lock, RW_READER);
	for (mod = fctl_ulp_modules; mod; mod = mod->mod_next) {
		ulp_port = fctl_get_ulp_port(mod, port);
		if (ulp_port == NULL) {
			continue;
		}

		mutex_enter(&ulp_port->port_mutex);
		if (FCTL_DISALLOW_CALLBACKS(ulp_port->port_dstate)) {
			mutex_exit(&ulp_port->port_mutex);
			continue;
		}

		switch (ulp_port->port_statec) {
		case FC_ULP_STATEC_DONT_CARE:
			if (ulp_port->port_statec != new_state) {
				ulp_port->port_statec = new_state;
			}
			break;

		case FC_ULP_STATEC_ONLINE:
		case FC_ULP_STATEC_OFFLINE:
			if (ulp_port->port_statec == new_state) {
				mutex_exit(&ulp_port->port_mutex);
				continue;
			}
			ulp_port->port_statec = new_state;
			break;

		case FC_ULP_STATEC_OFFLINE_TIMEOUT:
			if (ulp_port->port_statec == new_state ||
			    new_state == FC_ULP_STATEC_OFFLINE) {
				mutex_exit(&ulp_port->port_mutex);
				continue;
			}
			ulp_port->port_statec = new_state;
			break;

		default:
			ASSERT(0);
			break;
		}

		mod->mod_info->ulp_statec_callback(
		    mod->mod_info->ulp_handle, (opaque_t)port,
		    clist->clist_state, clist->clist_flags,
		    clist->clist_map, clist->clist_len, s_id);

		mutex_exit(&ulp_port->port_mutex);
	}
	rw_exit(&fctl_ulp_lock);

	if (clist->clist_size) {
		int 			count;
		fc_device_t		*node;
		fc_port_device_t	*pd;

		ASSERT(clist->clist_map != NULL);
		for (count = 0; count < clist->clist_len; count++) {
			if (clist->clist_map[count].map_state ==
			    PORT_DEVICE_INVALID) {
				pd = clist->clist_map[count].map_pd;
				ASSERT(pd != NULL);

				/*
				 * No need to be so respectful as to grab
				 * the mutex when it is dead already but
				 * it is not a big concern.
				 */
				mutex_enter(&pd->pd_mutex);
				node = pd->pd_device;
				mutex_exit(&pd->pd_mutex);

				fctl_remove_port_device(port, pd);
				fctl_remove_device(node);
			}
		}
		kmem_free(clist->clist_map,
		    sizeof (*(clist->clist_map)) * clist->clist_size);
	}

	kmem_free(clist, sizeof (*clist));
	return (1);
}


struct fc_device *
fctl_create_device(la_wwn_t *nwwn, int sleep)
{
	struct fc_device *device;

	device = kmem_zalloc(sizeof (*device), sleep);
	if (device != NULL) {
		mutex_init(&device->fd_mutex, NULL, MUTEX_DRIVER, NULL);
		mutex_enter(&device->fd_mutex);
		device->fd_node_name = *nwwn;
		device->fd_flags = FC_DEVICE_VALID;
		mutex_exit(&device->fd_mutex);
		if (fctl_enlist_nwwn_table(device, sleep) != FC_SUCCESS) {
			mutex_destroy(&device->fd_mutex);
			kmem_free(device, sizeof (*device));
			device = NULL;
		}
	}

	return (device);
}


void
fctl_remove_device(struct fc_device *device)
{
	mutex_enter(&device->fd_mutex);
	if (device->fd_count != 0 || device->fd_ports) {
		mutex_exit(&device->fd_mutex);
		return;
	}
	mutex_exit(&device->fd_mutex);
	fctl_delist_nwwn_table(device);
	mutex_destroy(&device->fd_mutex);
	kmem_free(device, sizeof (*device));
}


/*
 * Add the fc_device by this WWN to the nwwn_hash_table
 */
int
fctl_enlist_nwwn_table(struct fc_device *device, int sleep)
{
	int 		index;
	fc_nwwn_elem_t 	*new;
	fc_nwwn_list_t 	*head;

	ASSERT(!MUTEX_HELD(&device->fd_mutex));

	new = (fc_nwwn_elem_t *)kmem_zalloc(sizeof (*new), sleep);
	if (new == NULL) {
		return (FC_FAILURE);
	}
	mutex_enter(&nwwn_hash_mutex);
	new->fc_device = device;

	mutex_enter(&device->fd_mutex);
	index = HASH_FUNC(WWN_HASH_KEY(device->fd_node_name.raw_wwn),
		nwwn_table_size);
	mutex_exit(&device->fd_mutex);

	head = &nwwn_hash_table[index];

	new->hash_next = head->hash_head;
	head->hash_head = new;

	head->num_devs++;
	fctl_node_count++;
	mutex_exit(&nwwn_hash_mutex);

	return (FC_SUCCESS);
}


/*
 * Remove the fc_device by this WWN from the wwn_hash_list
 */
void
fctl_delist_nwwn_table(struct fc_device *device)
{
	int 		index;
	fc_nwwn_list_t 	*head;
	fc_nwwn_elem_t 	*elem;
	fc_nwwn_elem_t 	*prev;

	ASSERT(!MUTEX_HELD(&device->fd_mutex));

	mutex_enter(&nwwn_hash_mutex);

	mutex_enter(&device->fd_mutex);
	index = HASH_FUNC(WWN_HASH_KEY(device->fd_node_name.raw_wwn),
	    nwwn_table_size);
	mutex_exit(&device->fd_mutex);

	head = &nwwn_hash_table[index];
	elem = head->hash_head;
	prev = NULL;

	while (elem != NULL) {
		if (elem->fc_device == device) {
			head->num_devs--;
			if (prev == NULL) {
				head->hash_head = elem->hash_next;
			} else {
				prev->hash_next = elem->hash_next;
			}
			break;
		}
		prev = elem;
		elem = elem->hash_next;
	}
	fctl_node_count--;
	mutex_exit(&nwwn_hash_mutex);

	if (elem != NULL) {
		kmem_free(elem, sizeof (*elem));
	}
}


/*
 * Retrieve the fc_device by this WWN
 *
 * Note: The calling thread needs to make sure it isn't
 *		 holding any device mutex (more so the fc_device
 *		 that could potentially have this wwn).
 */
struct fc_device *
fctl_get_device_by_nwwn(la_wwn_t *node_wwn)
{
	int 			index;
	fc_nwwn_elem_t 		*elem;
	struct fc_device	*next;
	struct fc_device 	*device = NULL;

	index = HASH_FUNC(WWN_HASH_KEY(node_wwn->raw_wwn), nwwn_table_size);
	ASSERT(index >= 0 && index < nwwn_table_size);

	mutex_enter(&nwwn_hash_mutex);
	elem = nwwn_hash_table[index].hash_head;
	while (elem != NULL) {
		next = elem->fc_device;
		if (next != NULL) {
			mutex_enter(&next->fd_mutex);
			if (fctl_wwn_cmp(node_wwn, &next->fd_node_name) == 0) {
				device = next;
				mutex_exit(&next->fd_mutex);
				break;
			}
			mutex_exit(&next->fd_mutex);
		}
		elem = elem->hash_next;
	}
	mutex_exit(&nwwn_hash_mutex);

	return (device);
}


fc_port_device_t *
fctl_alloc_port_device(fc_port_t *port, la_wwn_t *port_wwn,
    uint32_t d_id, uchar_t recepient, int sleep)
{
	fc_port_device_t *pd;

	pd = kmem_zalloc(sizeof (*pd), sleep);
	if (pd == NULL) {
		return (pd);
	}
	mutex_init(&pd->pd_mutex, NULL, MUTEX_DRIVER, NULL);

	mutex_enter(&port->fp_mutex);
	mutex_enter(&pd->pd_mutex);

	pd->PD_PORT_ID = d_id;
	pd->pd_port_name = *port_wwn;
	pd->pd_port = port;
	pd->pd_state = PORT_DEVICE_VALID;
	pd->pd_type = PORT_DEVICE_NEW;
	pd->pd_recepient = recepient;

	fctl_enlist_did_table(port, pd);
	fctl_enlist_pwwn_table(port, pd);

	mutex_exit(&pd->pd_mutex);
	mutex_exit(&port->fp_mutex);

	return (pd);
}


void
fctl_dealloc_port_device(fc_port_device_t *pd)
{

#ifdef	DEBUG
	mutex_enter(&pd->pd_mutex);
	ASSERT(pd->pd_flags != PD_ELS_IN_PROGRESS);
	mutex_exit(&pd->pd_mutex);
#endif /* DEBUG */

	mutex_destroy(&pd->pd_mutex);
	kmem_free(pd, sizeof (*pd));
}


/*
 * Add Port device to the device handle
 */
void
fctl_add_port_to_device(struct fc_device *device, fc_port_device_t *pd)
{
	fc_port_device_t *last;
	fc_port_device_t *ports;

	mutex_enter(&device->fd_mutex);

	last = NULL;
	ports = device->fd_ports;
	while (ports != NULL) {
		if (ports == pd) {
			mutex_exit(&device->fd_mutex);
			return;
		}
		last = ports;
		ports = ports->pd_port_next;
	}

	if (last) {
		last->pd_port_next = pd;
	} else {
		device->fd_ports = pd;
	}
	pd->pd_port_next = NULL;

	mutex_enter(&pd->pd_mutex);
	pd->pd_device = device;
	mutex_exit(&pd->pd_mutex);

	device->fd_count++;
	mutex_exit(&device->fd_mutex);
}


void
fctl_remove_port_from_device(struct fc_device *device, fc_port_device_t *pd)
{
	fc_port_device_t *last;
	fc_port_device_t *ports;

	ASSERT(!MUTEX_HELD(&device->fd_mutex));
	ASSERT(!MUTEX_HELD(&pd->pd_mutex));

	last = NULL;

	mutex_enter(&device->fd_mutex);
	ports = device->fd_ports;
	while (ports != NULL) {
		if (ports == pd) {
			break;
		}
		last = ports;
		ports = ports->pd_port_next;
	}

	if (ports) {
		device->fd_count--;
		if (last) {
			last->pd_port_next = pd->pd_port_next;
		} else {
			device->fd_ports = pd->pd_port_next;
		}
		mutex_enter(&pd->pd_mutex);
		pd->pd_device = NULL;
		mutex_exit(&pd->pd_mutex);
	}
	pd->pd_port_next = NULL;
	mutex_exit(&device->fd_mutex);
}


void
fctl_enlist_did_table(fc_port_t *port, fc_port_device_t *pd)
{
	struct d_id_hash *head;

	ASSERT(MUTEX_HELD(&port->fp_mutex));
	ASSERT(MUTEX_HELD(&pd->pd_mutex));

	head = &port->fp_did_table[D_ID_HASH_FUNC(pd->PD_PORT_ID,
	    did_table_size)];

#ifdef	DEBUG
	{
		fc_port_device_t *tmp_pd;

		tmp_pd = head->d_id_head;
		while (tmp_pd != NULL) {
			ASSERT(tmp_pd != pd);
			tmp_pd = tmp_pd->pd_did_hnext;
		}
	}
#endif /* DEBUG */

	pd->pd_did_hnext = head->d_id_head;
	head->d_id_head = pd;

	head->d_id_count++;
}


void
fctl_delist_did_table(fc_port_t *port, fc_port_device_t *pd)
{
	uint32_t		d_id;
	struct d_id_hash 	*head;
	fc_port_device_t 	*pd_next;
	fc_port_device_t 	*last;

	ASSERT(MUTEX_HELD(&port->fp_mutex));
	ASSERT(MUTEX_HELD(&pd->pd_mutex));

	d_id = pd->PD_PORT_ID;
	head = &port->fp_did_table[D_ID_HASH_FUNC(d_id, did_table_size)];

	pd_next = head->d_id_head;
	last = NULL;
	while (pd_next != NULL) {
		if (pd == pd_next) {
			break;
		}
		last = pd_next;
		pd_next = pd_next->pd_did_hnext;
	}

	if (pd_next) {
		head->d_id_count--;
		if (last == NULL) {
			head->d_id_head = pd->pd_did_hnext;
		} else {
			last->pd_did_hnext = pd->pd_did_hnext;
		}
		pd->pd_did_hnext = NULL;
	}
}


void
fctl_enlist_pwwn_table(fc_port_t *port, fc_port_device_t *pd)
{
	int index;
	struct pwwn_hash *head;

	ASSERT(MUTEX_HELD(&port->fp_mutex));
	ASSERT(MUTEX_HELD(&pd->pd_mutex));

	index = HASH_FUNC(WWN_HASH_KEY(pd->pd_port_name.raw_wwn),
		pwwn_table_size);

	head = &port->fp_pwwn_table[index];

#ifdef	DEBUG
	{
		fc_port_device_t *tmp_pd;

		tmp_pd = head->pwwn_head;
		while (tmp_pd != NULL) {
			ASSERT(tmp_pd != pd);
			tmp_pd = tmp_pd->pd_wwn_hnext;
		}
	}
#endif /* DEBUG */

	pd->pd_wwn_hnext = head->pwwn_head;
	head->pwwn_head = pd;

	head->pwwn_count++;
}


void
fctl_delist_pwwn_table(fc_port_t *port, fc_port_device_t *pd)
{
	int			index;
	la_wwn_t		pwwn;
	struct pwwn_hash 	*head;
	fc_port_device_t 	*pd_next;
	fc_port_device_t 	*last;

	ASSERT(MUTEX_HELD(&port->fp_mutex));
	ASSERT(MUTEX_HELD(&pd->pd_mutex));

	pwwn = pd->pd_port_name;
	index = HASH_FUNC(WWN_HASH_KEY(pwwn.raw_wwn), pwwn_table_size);

	head = &port->fp_pwwn_table[index];

	last = NULL;
	pd_next = head->pwwn_head;
	while (pd_next != NULL) {
		if (pd_next == pd) {
			break;
		}
		last = pd_next;
		pd_next = pd_next->pd_wwn_hnext;
	}

	if (pd_next) {
		head->pwwn_count--;
		if (last == NULL) {
			head->pwwn_head = pd->pd_wwn_hnext;
		} else {
			last->pd_wwn_hnext = pd->pd_wwn_hnext;
		}
		pd->pd_wwn_hnext = NULL;
	}
}


fc_port_device_t *
fctl_get_port_device_by_did(fc_port_t *port, uint32_t d_id)
{
	struct d_id_hash 	*head;
	struct port_device 	*pd;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	mutex_enter(&port->fp_mutex);

	head = &port->fp_did_table[D_ID_HASH_FUNC(d_id, did_table_size)];

	pd = head->d_id_head;
	while (pd != NULL) {
		mutex_enter(&pd->pd_mutex);
		if (pd->PD_PORT_ID == d_id) {
			mutex_exit(&pd->pd_mutex);
			break;
		}
		mutex_exit(&pd->pd_mutex);
		pd = pd->pd_did_hnext;
	}

	mutex_exit(&port->fp_mutex);

	return (pd);
}


#ifndef	__lock_lint		/* uncomment when there is a consumer */

fc_port_device_t *
fctl_hold_port_device_by_did(fc_port_t *port, uint32_t d_id)
{
	struct d_id_hash 	*head;
	struct port_device 	*pd;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	mutex_enter(&port->fp_mutex);

	head = &port->fp_did_table[D_ID_HASH_FUNC(d_id, did_table_size)];

	pd = head->d_id_head;
	while (pd != NULL) {
		mutex_enter(&pd->pd_mutex);
		if (pd->PD_PORT_ID == d_id && pd->pd_state !=
		    PORT_DEVICE_INVALID && pd->pd_type != PORT_DEVICE_OLD) {
			pd->pd_held++;
			mutex_exit(&pd->pd_mutex);
			break;
		}
		mutex_exit(&pd->pd_mutex);
		pd = pd->pd_did_hnext;
	}

	mutex_exit(&port->fp_mutex);

	return (pd);
}

#endif /* __lock_lint */

fc_port_device_t *
fctl_get_port_device_by_pwwn(fc_port_t *port, la_wwn_t *pwwn)
{
	int			index;
	struct pwwn_hash 	*head;
	struct port_device 	*pd;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	mutex_enter(&port->fp_mutex);

	index = HASH_FUNC(WWN_HASH_KEY(pwwn->raw_wwn), pwwn_table_size);
	head = &port->fp_pwwn_table[index];

	pd = head->pwwn_head;
	while (pd != NULL) {
		mutex_enter(&pd->pd_mutex);
		if (fctl_wwn_cmp(&pd->pd_port_name, pwwn) == 0) {
			mutex_exit(&pd->pd_mutex);
			break;
		}
		mutex_exit(&pd->pd_mutex);
		pd = pd->pd_wwn_hnext;
	}

	mutex_exit(&port->fp_mutex);

	return (pd);
}


fc_port_device_t *
fctl_hold_port_device_by_pwwn(fc_port_t *port, la_wwn_t *pwwn)
{
	int			index;
	struct pwwn_hash 	*head;
	struct port_device 	*pd;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	mutex_enter(&port->fp_mutex);

	index = HASH_FUNC(WWN_HASH_KEY(pwwn->raw_wwn), pwwn_table_size);
	head = &port->fp_pwwn_table[index];

	pd = head->pwwn_head;
	while (pd != NULL) {
		mutex_enter(&pd->pd_mutex);
		if (fctl_wwn_cmp(&pd->pd_port_name, pwwn) == 0 &&
		    pd->pd_state != PORT_DEVICE_INVALID &&
		    pd->pd_type != PORT_DEVICE_OLD) {
			pd->pd_held++;
			mutex_exit(&pd->pd_mutex);
			break;
		}
		mutex_exit(&pd->pd_mutex);
		pd = pd->pd_wwn_hnext;
	}

	mutex_exit(&port->fp_mutex);

	return (pd);
}


void
fctl_release_port_device(fc_port_device_t *pd)
{
	mutex_enter(&pd->pd_mutex);
	ASSERT(pd->pd_held > 0);
	pd->pd_held--;
	mutex_exit(&pd->pd_mutex);
}


void
fctl_set_pd_state(fc_port_device_t *pd, int state)
{
	mutex_enter(&pd->pd_mutex);
	pd->pd_state = state;
	mutex_exit(&pd->pd_mutex);
}


int
fctl_get_pd_state(fc_port_device_t *pd)
{
	int state;

	mutex_enter(&pd->pd_mutex);
	state = pd->pd_state;
	mutex_exit(&pd->pd_mutex);

	return (state);
}


void
fctl_fillout_map(fc_port_t *port, fc_portmap_t **map, uint32_t *len,
    int whole_map)
{
	int			index;
	int			listlen;
	int			full_list;
	int			initiator;
	uint32_t		topology;
	struct pwwn_hash 	*head;
	fc_port_device_t 	*pd;
	fc_port_device_t 	*old_pd;
	fc_port_device_t	*last_pd;
	fc_portmap_t		*listptr;

	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	mutex_enter(&port->fp_mutex);
	topology = port->fp_topology;

	for (full_list = listlen = index = 0;
	index < pwwn_table_size; index++) {
		head = &port->fp_pwwn_table[index];
		pd = head->pwwn_head;
		while (pd != NULL) {
			full_list++;
			mutex_enter(&pd->pd_mutex);
			if (pd->pd_type != PORT_DEVICE_NOCHANGE) {
				listlen++;
			}
			mutex_exit(&pd->pd_mutex);
			pd = pd->pd_wwn_hnext;
		}
	}

	if (whole_map == 0) {
		if (listlen == 0 && *len == 0) {
			*map = NULL;
			*len = listlen;
			mutex_exit(&port->fp_mutex);
			return;
		}
	} else {
		if (full_list == 0 && *len == 0) {
			*map = NULL;
			*len = full_list;
			mutex_exit(&port->fp_mutex);
			return;
		}
	}

	if (*len == 0) {
		ASSERT(*map == NULL);
		if (whole_map == 0) {
			listptr = *map = kmem_zalloc(
			    sizeof (*listptr) * listlen, KM_SLEEP);
			*len = listlen;
		} else {
			listptr = *map = kmem_zalloc(
			    sizeof (*listptr) * full_list, KM_SLEEP);
			*len = full_list;
		}
	} else {
		/*
		 * By design this routine mandates the callers to
		 * ask for a whole map when they specify the length
		 * and the listptr.
		 */
		ASSERT(whole_map == 1);
		if (*len < full_list) {
			*len = full_list;
			mutex_exit(&port->fp_mutex);
			return;
		}
		listptr = *map;
		*len = full_list;
	}

	for (index = 0; index < pwwn_table_size; index++) {
		head = &port->fp_pwwn_table[index];
		last_pd = NULL;
		pd = head->pwwn_head;
		while (pd != NULL) {
			mutex_enter(&pd->pd_mutex);
			if ((whole_map == 0 &&
			    pd->pd_type == PORT_DEVICE_NOCHANGE) ||
			    pd->pd_state == PORT_DEVICE_INVALID) {
				mutex_exit(&pd->pd_mutex);
				last_pd = pd;
				pd = pd->pd_wwn_hnext;
				continue;
			}
			mutex_exit(&pd->pd_mutex);

			fctl_copy_portmap(listptr, pd);
			mutex_enter(&pd->pd_mutex);
			ASSERT(pd->pd_state != PORT_DEVICE_INVALID);
			if (pd->pd_type == PORT_DEVICE_OLD) {
				listptr->map_pd = pd;
				listptr->map_state = pd->pd_state =
				    PORT_DEVICE_INVALID;
				/*
				 * Remove this from the PWWN hash table.
				 */
				old_pd = pd;
				pd = old_pd->pd_wwn_hnext;

				if (last_pd == NULL) {
					ASSERT(old_pd == head->pwwn_head);

					head->pwwn_head = pd;
				} else {
					last_pd->pd_wwn_hnext = pd;
				}
				head->pwwn_count--;
				old_pd->pd_wwn_hnext = NULL;

				if (port->fp_topology == FC_TOP_PRIVATE_LOOP &&
				    port->fp_statec_busy) {
					fctl_check_alpa_list(port, old_pd);
				}

				/*
				 * Remove if the port device has stealthily
				 * present in the D_ID hash table
				 */
				fctl_delist_did_table(port, old_pd);

				ASSERT(old_pd->pd_device != NULL);

				initiator = (old_pd->pd_recepient ==
				    PD_PLOGI_INITIATOR) ? 1 : 0;

				mutex_exit(&old_pd->pd_mutex);
				mutex_exit(&port->fp_mutex);

				if (FC_IS_TOP_SWITCH(topology) && initiator) {
					(void) fctl_add_orphan(port, old_pd,
					    KM_NOSLEEP);
				}
				mutex_enter(&port->fp_mutex);
			} else {
				listptr->map_pd = pd;
				pd->pd_type = PORT_DEVICE_NOCHANGE;
				mutex_exit(&pd->pd_mutex);
				last_pd = pd;
				pd = pd->pd_wwn_hnext;
			}
			listptr++;
		}
	}
	mutex_exit(&port->fp_mutex);
}


job_request_t *
fctl_alloc_job(int job_code, int job_flags, void (*comp) (opaque_t, uchar_t),
    opaque_t arg, int sleep)
{
	job_request_t *job;

	job = (job_request_t *)kmem_cache_alloc(fctl_job_cache, sleep);
	if (job != NULL) {
		job->job_result = FC_SUCCESS;
		job->job_code = job_code;
		job->job_flags = job_flags;
		job->job_cb_arg = arg;
		job->job_comp = comp;
		job->job_private = NULL;
		job->job_ulp_pkts = NULL;
		job->job_ulp_listlen = 0;
#ifndef __lock_lint
		job->job_counter = 0;
		job->job_next = NULL;
#endif /* __lock_lint */
	}

	return (job);
}


void
fctl_dealloc_job(job_request_t *job)
{
	kmem_cache_free(fctl_job_cache, (void *)job);
}


void
fctl_enque_job(fc_port_t *port, job_request_t *job)
{
	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	mutex_enter(&port->fp_mutex);

	if (port->fp_job_tail == NULL) {
		ASSERT(port->fp_job_head == NULL);
		port->fp_job_head = port->fp_job_tail = job;
	} else {
		port->fp_job_tail->job_next = job;
		port->fp_job_tail = job;
	}
	cv_signal(&port->fp_cv);
	mutex_exit(&port->fp_mutex);
}


job_request_t *
fctl_deque_job(fc_port_t *port)
{
	job_request_t *job;

	ASSERT(MUTEX_HELD(&port->fp_mutex));

	if (port->fp_job_head == NULL) {
		ASSERT(port->fp_job_tail == NULL);
		job = NULL;
	} else {
		job = port->fp_job_head;
		if (job->job_next == NULL) {
			ASSERT(job == port->fp_job_tail);
			port->fp_job_tail = NULL;
		}
		port->fp_job_head = job->job_next;
	}

	return (job);
}


void
fctl_priority_enque_job(fc_port_t *port, job_request_t *job)
{
	ASSERT(!MUTEX_HELD(&port->fp_mutex));

	mutex_enter(&port->fp_mutex);
	if (port->fp_job_tail == NULL) {
		ASSERT(port->fp_job_head == NULL);
		port->fp_job_head = port->fp_job_tail = job;
	} else {
		job->job_next = port->fp_job_head;
		port->fp_job_head = job;
	}
	cv_signal(&port->fp_cv);
	mutex_exit(&port->fp_mutex);
}


void
fctl_jobwait(job_request_t *job)
{
	ASSERT(!(job->job_flags & JOB_TYPE_FCTL_ASYNC));
	sema_p(&job->job_fctl_sema);
	ASSERT(!MUTEX_HELD(&job->job_mutex));
}


void
fctl_jobdone(job_request_t *job)
{
	if (job->job_flags & JOB_TYPE_FCTL_ASYNC) {
		if (job->job_comp) {
			job->job_comp(job->job_cb_arg, job->job_result);
		}
		fctl_dealloc_job(job);
	} else {
		sema_v(&job->job_fctl_sema);
	}
}


/*
 * Simple match for two world wide names.
 *
 * The fctl_wwn_cmp doesn't include NAA
 * so if there is a need to compare two
 * WWN with NAA, use this; otherwise the
 * function fctl_wwn_cmp is good enough.
 *
 * Return 0 for the correct match case.
 */
int
fctl_wwn_match(la_wwn_t *src, la_wwn_t *dst)
{
	return (bcmp(src->raw_wwn, dst->raw_wwn, FCTL_WWN_SIZE(dst)));
}


/*
 * Compare two WWNs. The NAA is omitted for comparison.
 *
 * Note particularly that the indentation used in this
 * function  isn't according to Sun recommendations. It
 * is indented to make reading a bit easy.
 *
 * Return Values:
 *   if src == dst return  0
 *   if src > dst  return  1
 *   if src < dst  return -1
 */
int
fctl_wwn_cmp(la_wwn_t *src, la_wwn_t *dst)
{
	return (
	    (src->w.nport_id == dst->w.nport_id) ?
		((src->w.wwn_hi == dst->w.wwn_hi) ?
		    ((src->w.wwn_lo == dst->w.wwn_lo) ? 0 :
		    (src->w.wwn_lo > dst->w.wwn_lo) ? 1 : -1) :
		(src->w.wwn_hi > dst->w.wwn_hi) ? 1 : -1) :
	    (src->w.nport_id > dst->w.nport_id) ? 1 : -1);
}


/*
 * ASCII to Integer goodie with support
 * for base 16, 10, 2 and 8
 */
int
fctl_atoi(char *s, int base)
{
	int val;
	int ch;

	for (val = 0; *s != '\0'; s++) {
		switch (base) {
		case 16:
			if (*s >= '0' && *s <= '9') {
				ch = *s - '0';
			} else if (*s >= 'a' && *s <= 'f') {
				ch = *s - 'a' + 10;
			} else if (*s >= 'A' && *s <= 'F') {
				ch = *s - 'A' + 10;
			} else {
				return (-1);
			}
			break;

		case 10:
			if (*s < '0' || *s > '9') {
				return (-1);
			}
			ch = *s - '0';
			break;

		case 2:
			if (*s < '0' || *s > '1') {
				return (-1);
			}
			ch = *s - '0';
			break;

		case 8:
			if (*s < '0' || *s > '7') {
				return (-1);
			}
			ch = *s - '0';
			break;

		default:
			return (-1);
		}
		val = (val * base) + ch;
	}
	return (val);
}


/*
 * WWN to string goodie. Unpredictable results will happen
 * if enough memory isn't supplied in str argument. If you
 * are wondering how much does this routine need, it is just
 * (2 * WWN size + 1). So for a WWN size of 8 bytes the str
 * argument should have atleast 17 bytes allocated.
 */
void
fctl_wwn_to_str(la_wwn_t *wwn, caddr_t str)
{
	int count;

	for (count = 0; count < FCTL_WWN_SIZE(wwn); count++, str += 2) {
		(void) sprintf(str, "%02x", wwn->raw_wwn[count]);
	}
	str[(FCTL_WWN_SIZE(wwn)) << 1] = '\0';
}


/*
 * As a side effect this routine creates fc_device and
 * adds the fc_port_device_t to the fc_device
 */
fc_port_device_t *
fctl_create_port_device(fc_port_t *port, la_wwn_t *node_wwn,
    la_wwn_t *port_wwn, uint32_t d_id, uchar_t recepient, int sleep)
{
	int			invalid;
	fc_device_t		*device;
	fc_port_device_t 	*pd;

	pd = fctl_get_port_device_by_pwwn(port, port_wwn);
	if (pd) {
		mutex_enter(&pd->pd_mutex);
		invalid = (pd->pd_state == PORT_DEVICE_INVALID) ? 1 : 0;
		mutex_exit(&pd->pd_mutex);
	} else {
		invalid = 0;
	}
	if (pd == NULL || invalid) {
		pd = fctl_alloc_port_device(port, port_wwn,
		    d_id, recepient, sleep);
		if (pd == NULL) {
			return (pd);
		}
		mutex_enter(&port->fp_mutex);
		port->fp_dev_count++;
	} else {
		mutex_enter(&port->fp_mutex);

		mutex_enter(&pd->pd_mutex);
		ASSERT(pd->pd_device != NULL);

		if (pd->pd_type == PORT_DEVICE_OLD) {
			/*
			 * This one shouldn't be on the hash list
			 */
#ifdef	DEBUG
			mutex_exit(&pd->pd_mutex);
			mutex_exit(&port->fp_mutex);
			ASSERT(fctl_get_port_device_by_did(port, d_id) == NULL);
			mutex_enter(&port->fp_mutex);
			mutex_enter(&pd->pd_mutex);
#endif /* DEBUG */
			pd->pd_type = PORT_DEVICE_NOCHANGE;
			pd->pd_state = PORT_DEVICE_VALID;
			fctl_enlist_did_table(port, pd);
		} else if (pd->PD_PORT_ID != d_id) {
			char string[(FCTL_WWN_SIZE(port_wwn) << 1) + 1];
			uint32_t old_id;

			/*
			 * A very unlikely occurance in a well
			 * behaved environment.
			 */
			fctl_wwn_to_str(port_wwn, string);

			old_id = pd->PD_PORT_ID;
			fctl_delist_did_table(port, pd);

			cmn_err(CE_NOTE, "!fctl(%d): D_ID of a device"
			    " with PWWN %s changed. New D_ID = %x,"
			    " OLD D_ID = %x", port->fp_instance, string,
			    d_id, old_id);

			pd->PD_PORT_ID = d_id;
			pd->pd_type = PORT_DEVICE_CHANGED;
			fctl_enlist_did_table(port, pd);
		} else {
			/* sanitize device values */
			pd->pd_type = PORT_DEVICE_NOCHANGE;
			pd->pd_state = PORT_DEVICE_VALID;
		}
		mutex_exit(&pd->pd_mutex);
	}
	mutex_exit(&port->fp_mutex);

	device = fctl_get_device_by_nwwn(node_wwn);
	if (device) {
		mutex_enter(&device->fd_mutex);
		invalid = (device->fd_flags == FC_DEVICE_INVALID) ? 1 : 0;
		mutex_exit(&device->fd_mutex);
	}
	if (device == NULL || invalid) {
		device = fctl_create_device(node_wwn, sleep);
		if (device == NULL) {
			/*
			 * The following function remove the port
			 * device as a side effect as well.
			 */
			fctl_remove_port_device(port, pd);
			return (NULL);
		}
	}
	fctl_add_port_to_device(device, pd);

	return (pd);
}


void
fctl_remove_port_device(fc_port_t *port, fc_port_device_t *pd)
{
	fc_device_t *device;

	mutex_enter(&pd->pd_mutex);
	device = pd->pd_device;
	ASSERT(pd->pd_held == 0);
	mutex_exit(&pd->pd_mutex);

	if (device) {
		fctl_remove_port_from_device(device, pd);
	}

	mutex_enter(&port->fp_mutex);
	mutex_enter(&pd->pd_mutex);

	fctl_delist_did_table(port, pd);
	fctl_delist_pwwn_table(port, pd);
	port->fp_dev_count--;

	mutex_exit(&pd->pd_mutex);
	mutex_exit(&port->fp_mutex);

#ifdef	DEBUG
	{
		uint32_t	d_id;
		la_wwn_t	*pwwn;

		mutex_enter(&pd->pd_mutex);
		d_id = pd->PD_PORT_ID;
		pwwn = &pd->pd_port_name;
		mutex_exit(&pd->pd_mutex);

		ASSERT(fctl_get_port_device_by_did(port, d_id) != pd);
		ASSERT(fctl_get_port_device_by_pwwn(port, pwwn) != pd);
	}
#endif /* DEBUG */

	fctl_dealloc_port_device(pd);
}


void
fctl_remall(fc_port_t *port)
{
	int			index;
	fc_port_device_t	*pd;
	fc_device_t		*device;
	struct d_id_hash 	*head;

	mutex_enter(&port->fp_mutex);
	for (index = 0; index < did_table_size; index++) {
		head = &port->fp_did_table[index];
		while (head->d_id_head != NULL) {
			pd = head->d_id_head;
			mutex_enter(&pd->pd_mutex);
			device = pd->pd_device;
			mutex_exit(&pd->pd_mutex);
			if (device) {
				fctl_remove_port_from_device(device, pd);
				fctl_remove_device(device);
			}
			mutex_enter(&pd->pd_mutex);
			fctl_delist_pwwn_table(port, pd);
			mutex_exit(&pd->pd_mutex);
			head->d_id_head = pd->pd_did_hnext;
			fctl_dealloc_port_device(pd);
		}
	}
	mutex_exit(&port->fp_mutex);
}


int
fctl_is_wwn_zero(la_wwn_t *wwn)
{
	int count;

	for (count = 0; count < sizeof (la_wwn_t); count++) {
		if (wwn->raw_wwn[count] != 0) {
			return (FC_FAILURE);
		}
	}

	return (FC_SUCCESS);
}


void
fctl_ulp_unsol_cb(fc_port_t *port, fc_unsol_buf_t *buf, uchar_t type)
{
	int			data_cb;
	int			check_type;
	int			rval;
	uint32_t		claimed;
	fc_ulp_module_t 	*mod;
	fc_ulp_ports_t		*ulp_port;

	claimed = 0;
	check_type = 1;

	switch ((buf->ub_frame.r_ctl) & R_CTL_ROUTING) {
	case R_CTL_DEVICE_DATA:
		data_cb = 1;
		break;

	case R_CTL_EXTENDED_SVC:
		check_type = 0;
		/* FALLTHROUGH */

	case R_CTL_FC4_SVC:
		data_cb = 0;
		break;

	default:
		FC_RELEASE_AN_UB(port, buf);
		return;
	}

	rw_enter(&fctl_ulp_lock, RW_READER);
	for (mod = fctl_ulp_modules; mod; mod = mod->mod_next) {
		if (check_type && mod->mod_info->ulp_type != type) {
			continue;
		}

		ulp_port = fctl_get_ulp_port(mod, port);
		if (ulp_port == NULL) {
			continue;
		}

		mutex_enter(&ulp_port->port_mutex);
		if (FCTL_DISALLOW_CALLBACKS(ulp_port->port_dstate)) {
			mutex_exit(&ulp_port->port_mutex);
			continue;
		}
		mutex_exit(&ulp_port->port_mutex);

		if (data_cb == 1) {
			rval = mod->mod_info->ulp_data_callback(
			    mod->mod_info->ulp_handle,
			    (opaque_t)port, buf, claimed);
		} else {
			rval = mod->mod_info->ulp_els_callback(
			    mod->mod_info->ulp_handle,
			    (opaque_t)port, buf, claimed);
		}

		if (rval == FC_SUCCESS && claimed == 0) {
			claimed = 1;
		}
	}
	rw_exit(&fctl_ulp_lock);

	if (claimed == 0) {
		/*
		 * We should actually RJT since nobody claimed it.
		 */
		FC_RELEASE_AN_UB(port, buf);
	} else {
		mutex_enter(&port->fp_mutex);
		if (--port->fp_active_ubs == 0) {
			port->fp_soft_state &= ~FP_SOFT_IN_UNSOL_CB;
		}
		mutex_exit(&port->fp_mutex);
	}
}


void
fctl_copy_portmap(fc_portmap_t *map, fc_port_device_t *pd)
{
	fc_device_t *node;

	ASSERT(!MUTEX_HELD(&pd->pd_mutex));

	mutex_enter(&pd->pd_mutex);
	map->map_pwwn = pd->pd_port_name;
	map->map_did = pd->pd_port_id;
	map->map_hard_addr = pd->pd_hard_addr;
	map->map_state = pd->pd_state;
	map->map_flags = pd->pd_type;

	ASSERT(map->map_flags <= PORT_DEVICE_DELETE);

	bcopy(pd->pd_fc4types, map->map_fc4_types, sizeof (pd->pd_fc4types));

	node = pd->pd_device;
	mutex_exit(&pd->pd_mutex);

	if (node) {
		mutex_enter(&node->fd_mutex);
		map->map_nwwn = node->fd_node_name;
		mutex_exit(&node->fd_mutex);
	}
	map->map_pd = pd;
}


static int
fctl_update_host_ns_values(fc_port_t *port, fc_ns_cmd_t *ns_req)
{
	int 	rval = FC_SUCCESS;

	switch (ns_req->ns_cmd) {
	case NS_RFT_ID:
	{
		ns_rfc_type_t *rfc;

		rfc = (ns_rfc_type_t *)ns_req->ns_req_payload;

		mutex_enter(&port->fp_mutex);
		bcopy(rfc->rfc_types, port->fp_fc4_types,
		    sizeof (rfc->rfc_types));
		mutex_exit(&port->fp_mutex);

		break;
	}

	case NS_RSPN_ID:
	{
		ns_spn_t *spn;

		spn = (ns_spn_t *)ns_req->ns_req_payload;

		mutex_enter(&port->fp_mutex);
		port->fp_sym_port_namelen = spn->spn_len;
		if (spn->spn_len) {
			bcopy((caddr_t)spn + sizeof (ns_spn_t),
			    port->fp_sym_port_name, spn->spn_len);
		}
		mutex_exit(&port->fp_mutex);

		break;
	}

	case NS_RSNN_NN:
	{
		ns_snn_t *snn;

		snn = (ns_snn_t *)ns_req->ns_req_payload;

		mutex_enter(&port->fp_mutex);
		port->fp_sym_node_namelen = snn->snn_len;
		if (snn->snn_len) {
			bcopy((caddr_t)snn + sizeof (ns_snn_t),
			    port->fp_sym_node_name, snn->snn_len);
		}
		mutex_exit(&port->fp_mutex);

		break;
	}

	case NS_RIP_NN:
	{
		ns_rip_t *rip;

		rip = (ns_rip_t *)ns_req->ns_req_payload;

		mutex_enter(&port->fp_mutex);
		bcopy(rip->rip_ip_addr, port->fp_ip_addr,
		    sizeof (rip->rip_ip_addr));
		mutex_exit(&port->fp_mutex);

		break;
	}

	case NS_RIPA_NN:
	{
		ns_ipa_t *ipa;

		ipa = (ns_ipa_t *)ns_req->ns_req_payload;

		mutex_enter(&port->fp_mutex);
		bcopy(ipa->ipa_value, port->fp_ipa, sizeof (ipa->ipa_value));
		mutex_exit(&port->fp_mutex);

		break;
	}

	default:
		rval = FC_BADOBJECT;
		break;
	}

	return (rval);
}


static int
fctl_retrieve_host_ns_values(fc_port_t *port, fc_ns_cmd_t *ns_req)
{
	int 	rval = FC_SUCCESS;

	switch (ns_req->ns_cmd) {
	case NS_GFT_ID:
	{
		ns_rfc_type_t *rfc;

		rfc = (ns_rfc_type_t *)ns_req->ns_resp_payload;

		mutex_enter(&port->fp_mutex);
		bcopy(port->fp_fc4_types, rfc->rfc_types,
		    sizeof (rfc->rfc_types));
		mutex_exit(&port->fp_mutex);
		break;
	}

	case NS_GSPN_ID:
	{
		ns_spn_t *spn;

		spn = (ns_spn_t *)ns_req->ns_resp_payload;

		mutex_enter(&port->fp_mutex);
		spn->spn_len = port->fp_sym_port_namelen;
		if (spn->spn_len) {
			bcopy(port->fp_sym_port_name, (caddr_t)spn +
			    sizeof (ns_spn_t), spn->spn_len);
		}
		mutex_exit(&port->fp_mutex);

		break;
	}

	case NS_GSNN_NN:
	{
		ns_snn_t *snn;

		snn = (ns_snn_t *)ns_req->ns_resp_payload;

		mutex_enter(&port->fp_mutex);
		snn->snn_len = port->fp_sym_node_namelen;
		if (snn->snn_len) {
			bcopy(port->fp_sym_node_name, (caddr_t)snn +
			    sizeof (ns_snn_t), snn->snn_len);
		}
		mutex_exit(&port->fp_mutex);

		break;
	}

	case NS_GIP_NN:
	{
		ns_rip_t *rip;

		rip = (ns_rip_t *)ns_req->ns_resp_payload;

		mutex_enter(&port->fp_mutex);
		bcopy(port->fp_ip_addr, rip->rip_ip_addr,
		    sizeof (rip->rip_ip_addr));
		mutex_exit(&port->fp_mutex);

		break;
	}

	case NS_GIPA_NN:
	{
		ns_ipa_t *ipa;

		ipa = (ns_ipa_t *)ns_req->ns_resp_payload;

		mutex_enter(&port->fp_mutex);
		bcopy(port->fp_ipa, ipa->ipa_value, sizeof (ipa->ipa_value));
		mutex_exit(&port->fp_mutex);

		break;
	}

	default:
		rval = FC_BADOBJECT;
		break;
	}

	return (rval);
}


fctl_ns_req_t *
fctl_alloc_ns_cmd(uint32_t cmd_len, uint32_t resp_len, uint32_t data_len,
    uint32_t ns_flags, int sleep)
{
	fctl_ns_req_t *ns_cmd;

	ns_cmd = kmem_zalloc(sizeof (*ns_cmd), sleep);
	if (ns_cmd == NULL) {
		return (NULL);
	}

	if (cmd_len) {
		ns_cmd->ns_cmd_buf = kmem_zalloc(cmd_len, sleep);
		if (ns_cmd->ns_cmd_buf == NULL) {
			kmem_free(ns_cmd, sizeof (*ns_cmd));
			return (NULL);
		}
		ns_cmd->ns_cmd_size = cmd_len;
	}

	ns_cmd->ns_resp_size = resp_len;

	if (data_len) {
		ns_cmd->ns_data_buf = kmem_zalloc(data_len, sleep);
		if (ns_cmd->ns_data_buf == NULL) {
			if (ns_cmd->ns_cmd_buf && cmd_len) {
				kmem_free(ns_cmd->ns_cmd_buf, cmd_len);
			}
			kmem_free(ns_cmd, sizeof (*ns_cmd));
			return (NULL);
		}
		ns_cmd->ns_data_len = data_len;
	}
	ns_cmd->ns_flags = ns_flags;

	return (ns_cmd);
}


void
fctl_free_ns_cmd(fctl_ns_req_t *ns_cmd)
{
	if (ns_cmd->ns_cmd_size && ns_cmd->ns_cmd_buf) {
		kmem_free(ns_cmd->ns_cmd_buf, ns_cmd->ns_cmd_size);
	}
	if (ns_cmd->ns_data_len && ns_cmd->ns_data_buf) {
		kmem_free(ns_cmd->ns_data_buf, ns_cmd->ns_data_len);
	}
	kmem_free(ns_cmd, sizeof (*ns_cmd));
}


int
fctl_ulp_port_ioctl(fc_port_t *port, dev_t dev, int cmd,
    intptr_t data, int mode, cred_t *credp, int *rval)
{
	int			ret;
	int			save;
	uint32_t 		claimed;
	fc_ulp_module_t 	*mod;
	fc_ulp_ports_t		*ulp_port;

	save = *rval;
	*rval = ENOTTY;

	rw_enter(&fctl_ulp_lock, RW_READER);
	for (claimed = 0, mod = fctl_ulp_modules; mod; mod = mod->mod_next) {
		ulp_port = fctl_get_ulp_port(mod, port);
		if (ulp_port == NULL) {
			continue;
		}

		mutex_enter(&ulp_port->port_mutex);
		if (FCTL_DISALLOW_CALLBACKS(ulp_port->port_dstate) ||
		    mod->mod_info->ulp_port_ioctl == NULL) {
			mutex_exit(&ulp_port->port_mutex);
			continue;
		}
		mutex_exit(&ulp_port->port_mutex);

		ret = mod->mod_info->ulp_port_ioctl(
		    mod->mod_info->ulp_handle, (opaque_t)port,
		    dev, cmd, data, mode, credp, rval, claimed);

		if (ret == FC_SUCCESS && claimed == 0) {
			claimed = 1;
		}
	}
	rw_exit(&fctl_ulp_lock);

	ret = *rval;
	*rval = save;

	return (ret);
}


int
fctl_add_orphan(fc_port_t *port, fc_port_device_t *pd, int sleep)
{
	int		rval = FC_FAILURE;
	la_wwn_t	pwwn;
	fc_orphan_t	*orphan;

	orphan = kmem_zalloc(sizeof (*orphan), sleep);
	if (orphan) {
		mutex_enter(&pd->pd_mutex);
		pwwn = pd->pd_port_name;
		mutex_exit(&pd->pd_mutex);

		mutex_enter(&port->fp_mutex);

		orphan->orp_pwwn = pwwn;
		orphan->orp_tstamp = ddi_get_lbolt();

		if (port->fp_orphan_list) {
			ASSERT(port->fp_orphan_count > 0);
			orphan->orp_next = port->fp_orphan_list;
		}
		port->fp_orphan_list = orphan;
		port->fp_orphan_count++;
		mutex_exit(&port->fp_mutex);

		rval = FC_SUCCESS;
	}

	return (rval);
}


int
fctl_remove_if_orphan(fc_port_t *port, la_wwn_t *pwwn)
{
	int		rval = FC_FAILURE;
	fc_orphan_t	*prev = NULL;
	fc_orphan_t 	*orp;

	mutex_enter(&port->fp_mutex);
	for (orp = port->fp_orphan_list; orp != NULL; orp = orp->orp_next) {
		if (fctl_wwn_cmp(&orp->orp_pwwn, pwwn) == FC_SUCCESS) {
			if (prev) {
				prev->orp_next = orp->orp_next;
			} else {
				ASSERT(port->fp_orphan_list == orp);
				port->fp_orphan_list = orp->orp_next;
			}
			port->fp_orphan_count--;
			rval = FC_SUCCESS;
			break;
		}
		prev = orp;
	}
	mutex_exit(&port->fp_mutex);

	return (rval);
}


/* ARGSUSED */
static void
fctl_link_reset_done(opaque_t port_handle, uchar_t result)
{
	fc_port_t *port = (fc_port_t *)port_handle;

	mutex_enter(&port->fp_mutex);
	port->fp_soft_state &= ~FP_SOFT_IN_LINK_RESET;
	mutex_exit(&port->fp_mutex);
}


static int
fctl_error(int fc_errno, char **errmsg)
{
	int count;

	for (count = 0; count < sizeof (fc_errlist) /
	    sizeof (fc_errlist[0]); count++) {
		if (fc_errlist[count].fc_errno == fc_errno) {
			*errmsg = fc_errlist[count].fc_errname;
			return (FC_SUCCESS);
		}
	}
	*errmsg = fctl_undefined;

	return (FC_FAILURE);
}


/*
 * Return number of successful translations.
 *	Anybody with some userland programming experience would have
 *	figured it by now that the return value exactly resembles that
 *	of scanf(3c). This function returns a count of successful
 *	translations. It could range from 0 (no match for state, reason,
 *	action, expln) to 4 (successful matches for all state, reason,
 *	action, expln) and where translation isn't successful into a
 *	friendlier message the relevent field is set to "Undefined"
 */
static int
fctl_pkt_error(fc_packet_t *pkt, char **state, char **reason,
    char **action, char **expln)
{
	int		ret;
	int 		len;
	int		index;
	fc_pkt_error_t	*error;
	fc_pkt_reason_t	*reason_b;	/* Base pointer */
	fc_pkt_action_t	*action_b;	/* Base pointer */
	fc_pkt_expln_t	*expln_b;	/* Base pointer */

	ret = 0;
	*state = *reason = *action = *expln = fctl_undefined;

	len = sizeof (fc_pkt_errlist) / sizeof fc_pkt_errlist[0];
	for (index = 0; index < len; index++) {
		error = fc_pkt_errlist + index;
		if (pkt->pkt_state == error->pkt_state) {
			*state = error->pkt_msg;
			ret++;

			reason_b = error->pkt_reason;
			action_b = error->pkt_action;
			expln_b = error->pkt_expln;

			while (reason_b != NULL &&
			    reason_b->reason_val != FC_REASON_INVALID) {
				if (reason_b->reason_val == pkt->pkt_reason) {
					*reason = reason_b->reason_msg;
					ret++;
					break;
				}
				reason_b++;
			}

			while (action_b != NULL &&
			    action_b->action_val != FC_ACTION_INVALID) {
				if (action_b->action_val == pkt->pkt_action) {
					*action = action_b->action_msg;
					ret++;
					break;
				}
				action_b++;
			}

			while (expln_b != NULL &&
			    expln_b->expln_val != FC_EXPLN_INVALID) {
				if (expln_b->expln_val == pkt->pkt_expln) {
					*expln = expln_b->expln_msg;
					ret++;
					break;
				}
				expln_b++;
			}
			break;
		}
	}

	return (ret);
}


/*
 * Remove all port devices that are marked OLD, remove
 * corresponding node devices (fc_device_t)
 */
void
fctl_remove_oldies(fc_port_t *port)
{
	int			index;
	int			initiator;
	fc_device_t		*node;
	struct pwwn_hash 	*head;
	fc_port_device_t 	*pd;
	fc_port_device_t 	*old_pd;
	fc_port_device_t	*last_pd;

	/*
	 * Nuke all OLD devices
	 */
	mutex_enter(&port->fp_mutex);

	for (index = 0; index < pwwn_table_size; index++) {
		head = &port->fp_pwwn_table[index];
		last_pd = NULL;
		pd = head->pwwn_head;

		while (pd != NULL) {
			mutex_enter(&pd->pd_mutex);
			if (pd->pd_type != PORT_DEVICE_OLD) {
				mutex_exit(&pd->pd_mutex);
				last_pd = pd;
				pd = pd->pd_wwn_hnext;
				continue;
			}

			/*
			 * Remove this from the PWWN hash table
			 */
			old_pd = pd;
			pd = old_pd->pd_wwn_hnext;

			if (last_pd == NULL) {
				ASSERT(old_pd == head->pwwn_head);
				head->pwwn_head = pd;
			} else {
				last_pd->pd_wwn_hnext = pd;
			}
			head->pwwn_count--;
			old_pd->pd_wwn_hnext = NULL;

			fctl_delist_did_table(port, old_pd);
			node = old_pd->pd_device;
			ASSERT(node != NULL);

			initiator = (old_pd->pd_recepient ==
			    PD_PLOGI_INITIATOR) ? 1 : 0;

			mutex_exit(&old_pd->pd_mutex);

			if (FC_IS_TOP_SWITCH(port->fp_topology) && initiator) {
				mutex_exit(&port->fp_mutex);

				(void) fctl_add_orphan(port, old_pd,
				    KM_NOSLEEP);
			} else {
				mutex_exit(&port->fp_mutex);
			}

			fctl_remove_port_device(port, old_pd);
			fctl_remove_device(node);

			mutex_enter(&port->fp_mutex);
		}
	}

	mutex_exit(&port->fp_mutex);
}


static void
fctl_check_alpa_list(fc_port_t *port, fc_port_device_t *pd)
{
	ASSERT(MUTEX_HELD(&port->fp_mutex));
	ASSERT(port->fp_topology == FC_TOP_PRIVATE_LOOP);

	if (fctl_is_alpa_present(port, pd->PD_PORT_ID) == FC_SUCCESS) {
		return;
	}

	cmn_err(CE_WARN, "!fctl(%d): AL_PA=0x%x doesn't exist in LILP map",
	    port->fp_instance, pd->PD_PORT_ID);
}


static int
fctl_is_alpa_present(fc_port_t *port, uchar_t alpa)
{
	int index;

	ASSERT(MUTEX_HELD(&port->fp_mutex));
	ASSERT(port->fp_topology == FC_TOP_PRIVATE_LOOP);

	for (index = 0; index < port->fp_lilp_map.lilp_length; index++) {
		if (port->fp_lilp_map.lilp_alpalist[index] == alpa) {
			return (FC_SUCCESS);
		}
	}

	return (FC_FAILURE);
}
