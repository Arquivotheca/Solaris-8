/*
 * Copyright (c) 1993-2000 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CPR_H
#define	_SYS_CPR_H

#pragma ident	"@(#)cpr.h	1.77	99/11/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/obpdefs.h>
#include <sys/param.h>
#include <sys/mutex.h>
#include <sys/uadmin.h>
#include <sys/compress.h>

/*
 * definitions for kernel, cprboot, pmconfig
 */
#define	CPR_VERSION		4
#define	CPR_CONFIG		"/etc/.cpr_config"

/*
 * max(strlen("true"), strlen("false")) + 1
 */
#define	CPR_PROP_BOOL_LEN	6

/*
 * Saved values of nvram properties we modify.
 */
struct cprinfo {
	int	ci_magic;			/* magic word for booter */
	int	ci_reusable;			/* true if resuable statefile */
	char	ci_bootfile[OBP_MAXPATHLEN];	/* boot-file property */
	char	ci_bootdevice[OBP_MAXPATHLEN];	/* boot-device property */
	char	ci_autoboot[CPR_PROP_BOOL_LEN];	/* auto-boot? property */
	char	ci_diagfile[OBP_MAXPATHLEN];	/* diag-file property */
	char	ci_diagdevice[OBP_MAXPATHLEN];	/* diag-device property */
};

/*
 * cprinfo magic words
 */
#define	CPR_CONFIG_MAGIC	0x436E4667	/* 'CnFg' */
#define	CPR_DEFAULT_MAGIC	0x44664C74	/* 'DfLt' */


/*
 * Static information about the nvram properties we save/modify.
 */
struct prop_info {
	char	*pinf_name;
	int	pinf_len;
	int	pinf_offset;
};

#define	CPR_PROPINFO_INITIALIZER			\
{							\
	"boot-file", OBP_MAXPATHLEN,			\
	(int)offsetof(struct cprinfo, ci_bootfile),	\
	"boot-device", OBP_MAXPATHLEN,			\
	(int)offsetof(struct cprinfo, ci_bootdevice),	\
	"auto-boot?", CPR_PROP_BOOL_LEN,		\
	(int)offsetof(struct cprinfo, ci_autoboot),	\
	"diag-file", OBP_MAXPATHLEN,			\
	(int)offsetof(struct cprinfo, ci_diagfile),	\
	"diag-device", OBP_MAXPATHLEN,			\
	(int)offsetof(struct cprinfo, ci_diagdevice),	\
}

/*
 * Configuration info provided by user via pmconfig.
 *
 * The first part (cf_type, cf_path, cf_fs, cf_devfs, cf_dev_prom)
 * is used by both the cpr kernel module and cpr booter program
 * to locate the statefile.
 *
 * cf_type	CFT_UFS
 * cf_path	(path within file system) ".CPR"
 * cf_fs	(mount point for the statefile's filesystem) "/export/home"
 * cf_devfs	(devfs path of disk parition mounted there) "/dev/dsk/c0t0d0s7"
 * cf_dev_prom	(prom device path of the above disk partition)
 *			"/sbus/espdma/dma/sd@0:h"
 *
 * If the statefile were on a character special device (/dev//rdsk/c0t1d0s7),
 * the fields would have the typical values shown below:
 *
 * cf_type	CFT_SPEC
 * cf_path	ignored
 * cf_fs	ignored
 * cf_devfs	/dev/rdsk/c1t0d0s7
 * cf_dev_prom	(prom device path of the above special file)
 *			"/sbus/espdma/dma/sd@1:h"
 *
 * The rest of the fields are autoshutdown and autopm configuration related.
 * They are updated by pmconfig and consumed by both powerd and dtpower.
 */

struct cprconfig {
	int	cf_magic;			/* magic word for	*/
						/* booter to verify	*/
	int	cf_type;			/* CFT_UFS or CFT_SPEC	*/
	char	cf_path[MAXNAMELEN];		/* fs-relative path	*/
						/* for the state file	*/
	char	cf_fs[MAXNAMELEN];		/* mount point for fs	*/
						/* holding state file	*/
	char	cf_devfs[MAXNAMELEN];		/* path to device node	*/
						/* for above mount pt.	*/
	char	cf_dev_prom[OBP_MAXPATHLEN];	/* full device path of	*/
						/* above filesystem	*/
	/*
	 * autoshutdown configuration fields
	 */
	int	is_cpr_capable;			/* 0 - False, 1 - True */
	int	is_cpr_default;			/* 0 - False, 1 - True */
	int	is_autowakeup_capable;		/* 0 - False, 1 - True */
	int	as_idle;			/* idle time in min */
	int	as_sh;				/* Start_time hour */
	int	as_sm;				/* Start_time minutes */
	int	as_fh;				/* Finish_time hour */
	int	as_fm;				/* Finish_time minute */
	char	as_behavior[64];		/* "default","unconfigured", */
						/* "shutdown", "autowakeup" */
						/*  or "noshutdown" */
	int	ttychars_thold;			/* default = 0 */
	float	loadaverage_thold;		/* default = 0.04  */
	int	diskreads_thold;		/* default = 0 */
	int	nfsreqs_thold;			/* default = 0 */
	char	idlecheck_path[MAXPATHLEN];	/* default = "" */

	/*
	 * autopm behavior field
	 */
	int	is_autopm_default;		/* 0 - False, 1 - True */
	char	apm_behavior[64];		/* "enable","disable" or */
						/* "default" */
};


/*
 * values for cf_type
 */
#define	CFT_UFS		1		/* statefile is ufs file	*/
#define	CFT_SPEC	2		/* statefile is special file	*/



/*
 * definitions for kernel, cprboot
 */
#ifdef _KERNEL

#include <sys/promif.h>
#include <sys/sunddi.h>
#include <sys/sysmacros.h>
#include <sys/vnode.h>
#include <sys/cpr_impl.h>

extern int	cpr_debug;

#define	errp	prom_printf
#define	DPRINT

/*
 * DEBUG1 displays the main flow of CPR. Use it to identify which sub-module
 *	of CPR causes problems.
 * DEBUG2 displays minor stuff that normally won't matter.
 * DEBUG3 displays some big loops (cpr_dump); requires much longer runtime.
 * DEBUG4 displays lots of cprboot output, cpr_read and page handling.
 * DEBUG5 debug early machine dependant part of resume code.
 * DEBUG9 displays statistical data for CPR on console (by using printf),
 *	such as num page invalidated, etc.
 */
#define	LEVEL1		0x1
#define	LEVEL2		0x2
#define	LEVEL3		0x4
#define	LEVEL4		0x8
#define	LEVEL5		0x10
#define	LEVEL6		0x20
#define	LEVEL7		0x40
#define	LEVEL8		0x80

#define	DEBUG1(p)	{if (cpr_debug & LEVEL1) p; }
#define	DEBUG2(p)	{if (cpr_debug & LEVEL2) p; }
#define	DEBUG3(p)	{if (cpr_debug & LEVEL3) p; }
#define	DEBUG4(p)	{if (cpr_debug & LEVEL4) p; }
#define	DEBUG5(p)	{if (cpr_debug & LEVEL5) p; }
#define	DEBUG7(p)	{if (cpr_debug & LEVEL7) p; }
#define	DEBUG8(p)	{if (cpr_debug & LEVEL8) p; }
#define	DEBUG9(p)	{if (cpr_debug & LEVEL6) p; }

#define	CPR_DEBUG_BIT(dval)	(1 << (dval - AD_CPR_DEBUG0 - 1))


/*
 * CPR FILE FORMAT:
 *
 * 	Dump Header: general dump data:
 *		cpr_dump_desc
 *
 *	Machdep descriptor: cpr_machdep_desc
 *	Machdep data: sun4m/sun4u machine dependent info:
 *		cpr_sun4m_machdep
 *		cpr_sun4u_machdep, var length prom words
 *
 * 	Page Map: bitmap record consisting of a descriptor and data:
 *		cpr_bitmap_desc
 *		(char) bitmap[cpr_bitmap_desc.cbd_size]
 *
 * 	Page data: Contains one or more physical page records,
 *		each record consists of a descriptor and data:
 *		cpr_page_desc
 *		(char) page_data[cpr_page_desc.cpd_offset]
 *
 *	Terminator: end marker
 *		cpr_terminator
 *
 *	NOTE: cprboot now supports both ILP32 and LP64 kernels;
 *	the size of these structures written to a cpr statefile
 *	must be the same for ILP32 and LP64.  For details, see
 *	sun4u/sys/cpr_impl.h
 */

#define	CPR_DUMP_MAGIC		0x44754d70	/* 'DuMp' */
#define	CPR_BITMAP_MAGIC	0x42744d70	/* 'BtMp' */
#define	CPR_PAGE_MAGIC		0x50614765	/* 'PaGe' */
#define	CPR_MACHDEP_MAGIC	0x4d614470	/* 'MaDp' */
#define	CPR_TERM_MAGIC		0x5465526d	/* 'TeRm' */

/*
 * header at the begining of the dump data section
 */
struct cpr_dump_desc {
	uint_t		cdd_magic;	/* paranoia check */
	ushort_t	cdd_version;	/* version number */
	ushort_t	cdd_machine;	/* sun4m, sun4u */
	int		cdd_bitmaprec;	/* number of bitmap records */
	int		cdd_dumppgsize;	/* total # of frames dumped, in pages */
	int		cdd_test_mode;	/* true if called by uadmin test mode */
	int		cdd_debug;	/* turn on debug in cprboot */
	cpr_ext		cdd_filesize;	/* statefile size in bytes */
};
typedef struct cpr_dump_desc cdd_t;

/*
 * physical memory bitmap descriptor, preceeds the actual bitmap.
 */
struct cpr_bitmap_desc {
	uint_t		cbd_magic;	/* so we can spot it better */
	uint_t		cbd_spfn;   	/* starting pfn */
	uint_t		cbd_epfn;	/* ending pfn */
	uint_t		cbd_size;	/* size of this bitmap, in bytes */
	cpr_ptr		cbd_reg_bitmap;	/* regular bitmap */
	cpr_ptr		cbd_vlt_bitmap; /* volatile bitmap */
	cpr_ptr		cbd_auxmap; 	/* aux bitmap used during thaw */
};
typedef struct cpr_bitmap_desc cbd_t;

/*
 * Describes the contiguous pages saved in the storage area.
 * To save space data will be compressed before saved.
 * However some data end up bigger after compression.
 * In that case, we save the raw data and make a note
 * of it in the csd_clean_compress field.
 */
struct cpr_storage_desc {
	uint_t		csd_dirty_spfn;		/* starting dirty pfn */
	uint_t		csd_dirty_npages;
	cpr_ptr		csd_clean_sva;		/* starting clean va */
	uint_t		csd_clean_sz;		/* XXX Big enough??? */
	int		csd_clean_compressed;
#ifdef DEBUG
	uint_t		csd_usum;
	uint_t		csd_csum;
#endif
};
typedef struct cpr_storage_desc csd_t;

/*
 * Describes saved pages, preceeds page data;
 * cpd_lenth len is important when pages are compressed.
 */
struct cpr_page_desc {
	uint_t cpd_magic;	/* so we can spot it better */
	uint_t cpd_pfn;   	/* kern physical address page # */
	uint_t cpd_pages;	/* number of contiguous pages */
	uint_t cpd_length;	/* data segment size in bytes */
	uint_t cpd_flag;	/* see below */
	uint_t cpd_csum;	/* "after compression" checksum */
	uint_t cpd_usum;	/* "before compression" checksum */
};
typedef struct cpr_page_desc cpd_t;

/*
 * cpd_flag values
 */
#define	CPD_COMPRESS	0x0001	/* set if compressed */
#define	CPD_CSUM	0x0002	/* set if "after compression" checsum valid */
#define	CPD_USUM	0x0004	/* set if "before compression" checsum valid */

/*
 * machdep header stores the length of the platform specific information
 * that are used by resume.
 *
 * Note: the md_size field is the total length of the machine dependent
 * information.  This always includes a fixed length section and may
 * include a variable length section following it on some platforms.
 */
struct cpr_machdep_desc {
	uint_t md_magic;	/* paranoia check */
	uint_t md_size;		/* the size of the "opaque" data following */
};
typedef struct cpr_machdep_desc cmd_t;

typedef struct timespec32 cpr_time_t;

struct cpr_terminator {
	uint_t magic;			/* paranoia check */
	uint_t real_statef_size;	/* ...in bytes */
	cpr_ptr va;			/* virtual addr of this struct */
	cpr_ext pfn;			/* phys addr of this struct */
	cpr_time_t tm_shutdown;		/* time in milisec when shutdown */
	cpr_time_t tm_cprboot_start;	/* time when cprboot starts to run */
	cpr_time_t tm_cprboot_end;	/* time before jumping to kernel */
};
typedef struct cpr_terminator ctrm_t;


#define	REGULAR_BITMAP	(char *)CPR->c_bitmap_desc.cbd_reg_bitmap
#define	VOLATILE_BITMAP	(char *)CPR->c_bitmap_desc.cbd_vlt_bitmap

#define	WRITE_TO_STATEFILE	0
#define	SAVE_TO_STORAGE		1
#define	STORAGE_DESC_ALLOC	2


/*
 * prom_read() max is 32k
 * for sun4m, page size is 4k, CPR_MAXCONTIG is 8
 * for sun4u, page size is 8k, CPR_MAXCONTIG is 4
 */
#define	PROM_MAX_READ	0x8000
#define	CPR_MAX_BLOCK	0x8000
#define	CPR_MAXCONTIG	(CPR_MAX_BLOCK / MMU_PAGESIZE)

#define	PAGE_ROUNDUP(val)	(((val) + MMU_PAGEOFFSET) & MMU_PAGEMASK)


/*
 * redefinitions of uadmin subcommands for A_FREEZE
 */
#define	AD_CPR_COMPRESS		AD_COMPRESS /* store state file compressed */
#define	AD_CPR_FORCE		AD_FORCE /* force to do AD_CPR_COMPRESS */
#define	AD_CPR_CHECK		AD_CHECK /* test if CPR module is there */
#define	AD_CPR_REUSEINIT	AD_REUSEINIT /* write cprinfo file */
#define	AD_CPR_REUSABLE		AD_REUSABLE /* create reusable statefile */
#define	AD_CPR_REUSEFINI	AD_REUSEFINI /* revert to non-reusable CPR */
#define	AD_CPR_TESTHALT		6	/* test mode, halt */
#define	AD_CPR_TESTNOZ		7	/* test mode, auto-restart uncompress */
#define	AD_CPR_TESTZ		8	/* test mode, auto-restart compress */
#define	AD_CPR_PRINT		9	/* print out stats */
#define	AD_CPR_NOCOMPRESS	10	/* store state file uncompressed */
#define	AD_CPR_DEBUG0		100	/* clear debug flag */
#define	AD_CPR_DEBUG1		101	/* display CPR main flow via prom */
#define	AD_CPR_DEBUG2		102	/* misc small/mid size loops */
#define	AD_CPR_DEBUG3		103	/* exhaustive big loops */
#define	AD_CPR_DEBUG4		104	/* debug cprboot */
#define	AD_CPR_DEBUG5		105	/* debug machdep part of resume */
#define	AD_CPR_DEBUG7		107	/* debug bitmap code */
#define	AD_CPR_DEBUG8		108
#define	AD_CPR_DEBUG9		109	/* display stat data on console */

/*
 * cprboot related information and definitions.
 * The statefile names are hardcoded for now.
 */
#define	CPR_DEFAULT		"/.cpr_default"
#define	CPR_STATE_FILE		"/.CPR"
#define	CPR_NVRAMRC_LEN		1024


/*
 * definitions for CPR statistics
 */
#define	CPR_E_NAMELEN		64
#define	CPR_E_MAX_EVENTNUM	64

struct cpr_tdata {
	time_t	mtime;		/* mean time on this event */
	time_t	stime;		/* start time on this event */
	time_t	etime;		/* end time on this event */
	time_t	ltime;		/* time duration of the last event */
};
typedef struct cpr_tdata ctd_t;

struct cpr_event {
	struct	cpr_event *ce_next;	/* next event in the list */
	long	ce_ntests;		/* num of the events since loaded */
	ctd_t	ce_sec;			/* cpr time in sec on this event */
	ctd_t	ce_msec;		/* cpr time in 100*millisec */
	char 	ce_name[CPR_E_NAMELEN];
};

struct cpr_stat {
	int	cs_ntests;		/* num of cpr's since loaded */
	int	cs_mclustsz;		/* average cluster size: all in bytes */
	int	cs_upage2statef;	/* actual # of upages gone to statef */
	int	cs_min_comprate;	/* minimum compression ratio * 100 */
	pgcnt_t	cs_nosw_pages;		/* # of pages of no backing store */
	size_t	cs_nocomp_statefsz;	/* statefile size without compression */
	size_t	cs_est_statefsz;	/* estimated statefile size */
	size_t	cs_real_statefsz;	/* real statefile size */
	size_t	cs_dumped_statefsz;	/* how much has been dumped out */
	struct cpr_event *cs_event_head; /* The 1st one in stat event list */
	struct cpr_event *cs_event_tail; /* The last one in stat event list */
};

/*
 * macros for CPR statistics evaluation
 */
#define	CPR_STAT_EVENT_START(s)		cpr_stat_event_start(s, 0)
#define	CPR_STAT_EVENT_END(s)		cpr_stat_event_end(s, 0)
/*
 * use the following is other time zone is required
 */
#define	CPR_STAT_EVENT_START_TMZ(s, t)	cpr_stat_event_start(s, t)
#define	CPR_STAT_EVENT_END_TMZ(s, t)	cpr_stat_event_end(s, t)

#define	CPR_STAT_EVENT_PRINT		cpr_stat_event_print


/*
 * State Structure for CPR
 */
typedef struct cpr {
	uint_t		c_cprboot_magic;
	uint_t		c_flags;
	int		c_substate;	/* tracking suspend progress */
	int		c_fcn;		/* uadmin subcommand */
	vnode_t		*c_vp;		/* vnode for statefile */
	cbd_t  		c_bitmap_desc;
	caddr_t		c_mapping_area;	/* reserve for dumping kas phys pages */
	struct cpr_stat	c_stat;
	char		c_alloc_cnt;	/* # of statefile alloc retries */
} cpr_t;

/*
 * c_flags definitions
 */
#define	C_SUSPENDING		0x01
#define	C_RESUMING		0x02
#define	C_COMPRESSING		0x04
#define	C_REUSABLE		0x08
#define	C_ERROR			0x10

extern cpr_t cpr_state;
#define	CPR	(&cpr_state)
#define	STAT	(&cpr_state.c_stat)

/*
 * definitions for c_substate. It works together w/ c_flags to determine which
 * stages the CPR is at.
 */
#define	C_ST_USER_THREADS	0
#define	C_ST_STATEF_ALLOC	1
#define	C_ST_STATEF_ALLOC_RETRY	2
#define	C_ST_DDI		3
#define	C_ST_DRIVERS		4
#define	C_ST_DUMP		5
#define	C_ST_KERNEL_THREADS	6
#define	C_ST_REUSABLE		7

#define	cpr_set_substate(a)	(CPR->c_substate = (a))

#define	C_VP		(CPR->c_vp)

#define	C_MAX_ALLOC_RETRY	4

#define	CPR_PROM_SAVE		0
#define	CPR_PROM_RESTORE	1
#define	CPR_PROM_FREE		2

#define	CPRBUFS		256			/* no. of cpr write buffer */
#define	CPRBUFSZ	(CPRBUFS * DEV_BSIZE)	/* cpr write buffer size */

typedef int (*bitfunc_t)(pfn_t, char *);

/*
 * arena scan info
 */
struct cpr_walkinfo {
	char *bitmap;
	bitfunc_t bitfunc;
	pgcnt_t pages;
	size_t size;
	int ranges;
};


#ifndef _ASM

extern char *cpr_enumerate_promprops(char **, size_t *);
extern char *cpr_get_statefile_prom_path(void);
extern int cpr_change_mp(int);
extern int cpr_clrbit(pfn_t, char *);
extern int cpr_contig_pages(vnode_t *, int);
extern int cpr_dump(vnode_t *);
extern int cpr_get_bootinfo(struct cprinfo *);
extern int cpr_get_reusable_mode(void);
extern int cpr_main(void);
extern int cpr_mp_offline(void);
extern int cpr_mp_online(void);
extern int cpr_nobit(pfn_t, char *);
extern int cpr_read_cdump(int, cdd_t *, ushort_t);
extern int cpr_read_cprinfo(int, char *, char *);
extern int cpr_read_elf(int);
extern int cpr_read_machdep(int, caddr_t, size_t);
extern int cpr_read_phys_page(int, uint_t, int *);
extern int cpr_read_terminator(int, ctrm_t *, caddr_t);
extern int cpr_resume_devices(dev_info_t *);
extern int cpr_set_bootinfo(struct cprinfo *);
extern int cpr_set_properties(struct cprinfo *);
extern int cpr_setbit(pfn_t, char *);
extern int cpr_statefile_is_spec(void);
extern int cpr_stop_kernel_threads(void);
extern int cpr_stop_user_threads(void);
extern int cpr_suspend_devices(dev_info_t *);
extern int cpr_validate_cprinfo(struct cprinfo *, int);
extern int cpr_write(vnode_t *, caddr_t, int);
extern int i_cpr_dump_sensitive_kpages(vnode_t *);
extern int i_cpr_save_sensitive_kpages(void);
extern pgcnt_t cpr_count_kpages(char *, bitfunc_t);
extern pgcnt_t cpr_count_pages(caddr_t, size_t, char *, bitfunc_t);
extern pgcnt_t cpr_count_seg_pages(char *, bitfunc_t);
extern pgcnt_t cpr_count_volatile_pages(char *, bitfunc_t);
extern pgcnt_t cpr_scan_kvseg(char *, bitfunc_t);
extern pgcnt_t i_cpr_count_sensitive_kpages(char *, bitfunc_t);
extern pgcnt_t i_cpr_count_special_kpages(char *, bitfunc_t);
extern pgcnt_t i_cpr_count_storage_pages(char *, bitfunc_t);
extern ssize_t cpr_get_machdep_len(int);
extern void cpr_clear_cprinfo(struct cprinfo *);
extern void cpr_hold_driver(void);  /* callback for stopping drivers */
extern void cpr_reset_bootinfo(struct cprinfo *);
extern void cpr_restore_time(void);
extern void cpr_save_time(void);
extern void cpr_set_nvram(char *, char *, char *);
extern void cpr_show_range(char *, size_t, char *, bitfunc_t, pgcnt_t);
extern void cpr_signal_user(int sig);
extern void cpr_spinning_bar(void);
extern void cpr_start_user_threads(void);
extern void cpr_stat_cleanup(void);
extern void cpr_stat_event_end(char *, cpr_time_t *);
extern void cpr_stat_event_print(void);
extern void cpr_stat_event_start(char *, cpr_time_t *);
extern void cpr_stat_record_events(void);
extern void cpr_tod_get(cpr_time_t *ctp);

extern cpr_time_t wholecycle_tv;
extern int cpr_reusable_mode;

#endif	/* _ASM */
#endif	/* _KERNEL */


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CPR_H */
