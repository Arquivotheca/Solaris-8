/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)iostat.h 1.5	99/12/01 SMI"


#include <sys/isa_defs.h>
#include <kstat.h>

#define	DISK_OLD		0x0001
#define	DISK_NEW		0x0002
#define	DISK_EXTENDED		0x0004
#define	DISK_ERRORS		0x0008
#define	DISK_EXTENDED_ERRORS	0x0010
#define	DISK_NORMAL		(DISK_OLD | DISK_NEW)

#define	DISK_IO_MASK		(DISK_OLD | DISK_NEW | DISK_EXTENDED)
#define	DISK_ERROR_MASK		(DISK_ERRORS | DISK_EXTENDED_ERRORS)
#define	PRINT_VERTICAL		(DISK_ERROR_MASK | DISK_EXTENDED)

#define	REPRINT 19

/*
 * Name and print priority of each supported ks_class.
 */
#define	IO_CLASS_DISK		0
#define	IO_CLASS_PARTITION	0
#define	IO_CLASS_TAPE		1
#define	IO_CLASS_NFS		2

/*
 * It's really a pseudo-gigabyte. We use 1000000000 bytes so that the disk
 * labels don't look bad. 1GB is really 1073741824 bytes.
 */
#define	DISK_GIGABYTE   1000000000.0

#define	NAME_BUFLEN	256

/*
 * The following are used to control treatment of kstat names
 * which fall beyond the number of disk partitions allowed on
 * the particular ISA. PARTITIONS_OK is set only on an Intel
 * system.
 */
#define	SLICES_OK	1
#define	PARTITIONS_OK	2

/*
 * Description of each device identified
 */
typedef struct list_of_disks {
	char	*dtype;		/* device type: sd, ssd, md, st, etc. */
	int	dnum;		/* device number */
	char	*dsk;		/* in form of cNtNdN */
	char	*dname;		/* in form of /dev/dsk/cNtNdN */
	uint_t	flags;		/* see SLICES_OK and PARTITIONS_OK above */
	int	devtype;	/* disk, metadevice, tape */
	uint_t	seen;		/* Used for diffing disk lists */
	struct list_of_disks *next;	/* link to next one */
} disk_list_t;

/*
 * CPU kstat information. We need to save the kstat when we do
 * comparisons.
 */
struct cpu_sinfo {
	kstat_t *ksp;		/* Pointer to kstat */
	uint_t seen;		/* Did we see this kstat the next time? */
	kstat_t rksp;		/* buffer with entire kstat */
};

/*
 * Which kstat classes are we interested in and what is their
 * relative priority when we sort them.
 */
struct io_class {
	char    *class_name;
	int	class_priority;
};

/*
 * Detailed device information.
 */
typedef struct diskinfo {
	struct diskinfo *next;
	kstat_t 	ks;
	kstat_t 	*ksp;
	kstat_io_t 	new_kios;
	kstat_io_t	old_kios;
	hrtime_t	last_snap;
	int		selected;
	int		class;
	char    	*device_name;
	char    	*dname;		/* in form of /dev/dsk/cNtNdN */
	int 		seen;
	kstat_t		*disk_errs;	/* pointer to the disk's error kstats */
	struct diskinfo *cn;	/* pointer to next one in controller chain */
} diskinfo_t;

/*
 * Description of each mount point currently existing on the system.
 */
typedef struct mnt_info {
	char *device_name;
	char *mount_point;
	char *devinfo;
	uint_t minor;
	struct mnt_info *next;
} mnt_t;

/*
 * A basic description of each device found
 * on the system by walking the device tree.
 * These entries are used to select the
 * relevent entries from the actual /dev
 * entries.
 */
typedef struct ldinfo {
	char *name;
	char *dtype;
	int dnum;
	struct ldinfo *next;
} ldinfo_t;

/*
 * Optimization for lookup of kstats.
 * For each kstat prefix (e.g., 'sd')
 * found in a directory one of these
 * structures will be created.
 *
 * name: prefix of kstat name (e.g., 'ssd')
 * min:  smallest number seen from kstat
 *       name (e.g., 101 from 'sd101')
 * max:  largest number seen from kstat
 * list_start: beginning of disk_list structures
 * 	for this kstat type in the main list for
 *	this directory
 * list_end: end of entries for this kstat type
 * 	in this directory.
 */
typedef struct dev_name {
	char *name;
	uint_t min;
	uint_t max;
	disk_list_t *list_start;
	disk_list_t *list_end;
	struct dev_name *next;
} dev_name_t;

/*
 * Definition of a "type" of disk device.
 * Tied to the directory containing entries
 * for that device. Divides the list of
 * devices into localized chunks and allows
 * quick determination as to whether an entry
 * exists or whether we need to look at the
 * devices upon a state change.
 */
typedef struct dir_info {
	char *name;		/* directory name */
	time_t mtime;		/* mod time */
	disk_list_t *list;	/* master list of devices */
	dev_name_t *nf;		/* lists per name */
	uint_t skip_lookup;	/* skip lookup if device */
				/* does not have partitions */
	char *dtype;		/* Type of device */
	char *trimstr;		/* What do we prune */
	char  trimchr;		/* Char denoting end */
				/* of interesting data */
} dir_info_t;

/*
 * Function desciptor to be called when extended
 * headers are used.
 */
typedef struct formatter {
	void (*nfunc)(void);
	struct formatter *next;
} format_t;

/*
 * List of controllers seen on the system. A 'controller'
 * is defined as the value in /dev/dsk/cN where N is one
 * or more numeric characters. Only used when -C is
 * specified.
 */
typedef struct controller_info {
	uint_t cid;			/* value of N from cN */
	diskinfo_t *d;			/* list of disks */
	struct controller_info *next;	/* next controller */
} con_t;

void do_mnttab(void);
mnt_t *lookup_mntent_byname(char *);
disk_list_t *lookup_ks_name(char *, dir_info_t *);
void build_disk_list(void);
void safe_alloc(void **, uint_t, int);
void safe_strdup(char *, char **);
char *lookup_nfs_name(char *);
void fail(int, char *, ...);
