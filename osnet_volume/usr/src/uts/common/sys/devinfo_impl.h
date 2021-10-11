/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DEVINFO_IMPL_H
#define	_SYS_DEVINFO_IMPL_H

#pragma ident	"@(#)devinfo_impl.h	1.6	99/09/10 SMI"

/*
 * This file is separate from libdevinfo.h because the devinfo driver
 * needs to know about the stuff. Library consumer should not care
 * about stuff defined here.
 *
 * The only exception is di_priv_data (consolidation private) and
 * DINFO* ioctls.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/* ioctl commands for devinfo driver */

#define	DIIOC	(0xdf<<8)

/*
 * Any combination of the following ORed together will take a snapshot
 * of the device configuration data.
 *
 * XXX First three are public, last three are private
 */
#define	DINFOSUBTREE	(DIIOC | 0x01)	/* include subtree */
#define	DINFOMINOR	(DIIOC | 0x02)	/* include minor data */
#define	DINFOPROP	(DIIOC | 0x04)	/* include properties */

/* private bits */
#define	DINFOPRIVDATA	(DIIOC | 0x10)	/* include private data */
#define	DINFOFORCE	(DIIOC | 0x20)	/* force load all drivers */

/*
 * Straight ioctl commands, not bitwise operation
 */
#define	DINFOUSRLD	(DIIOC | 0x80)	/* copy snapshot to usrland */
#define	DINFOLODRV	(DIIOC | 0x81)	/* force load a driver */
#define	DINFOIDENT	(DIIOC | 0x82)	/* identify the driver */

/*
 * ioctl for taking a snapshot a single node and all nodes
 */
#define	DINFOCPYONE	DIIOC
#define	DINFOCPYALL	(DINFOSUBTREE | DINFOPROP | DINFOMINOR)

#define	DI_MAGIC	0xdfdf	/* magic number returned by DINFOIDENT */

/* driver ops encoding */

#define	DI_BUS_OPS	0x1
#define	DI_CB_OPS	0x2
#define	DI_STREAM_OPS	0x4

/* property list enumeration */

#define	DI_PROP_DRV_LIST	0
#define	DI_PROP_SYS_LIST	1
#define	DI_PROP_GLB_LIST	2
#define	DI_PROP_HW_LIST		3

/* misc parameters */

#define	MAX_TREE_DEPTH	64
#define	MAX_PTR_IN_PRV	5
#define	DI_SNAPSHOT_VERSION_0	0	/* reserved */
#define	DI_PRIVDATA_VERSION_0	10	/* Start from 10 so caller must set */
#define	DI_BIG_ENDIAN		0	/* reserved */
#define	DI_LITTLE_ENDIAN	1	/* reserved */

#define	DINO(addr)	((struct di_node *)(addr))
#define	DIMI(addr)	((struct di_minor *)(addr))
#define	DIPROP(addr)	((struct di_prop *)(addr))

typedef int32_t di_off_t;

/*
 * devinfo driver snapshot data structure
 */
struct di_all {
	int	version;	/* snapshot version, reserved */
	int	pd_version;	/* private data format version */
	int	endianness;	/* reserved for future use */
	int	generation;	/* reserved for future use */
	di_off_t	top_devinfo;
	di_off_t	devnames;
	di_off_t	ppdata_format;	/* parent priv data format array */
	di_off_t	dpdata_format;	/* driver priv data format array */
	int	n_ppdata;	/* size of ppdata_format array */
	int	n_dpdata;	/* size of pddata_format array */
	int	devcnt;		/* size of devnames array */
	uint_t	command;	/* same as in di_init() */
	uint_t	map_size;	/* size of the snapshot */
	char	root_path[1];	/* path to snapshot root */
};

struct di_devnm {
	di_off_t name;
	di_off_t global_prop;
	di_off_t head;	/* head of per instance list */
	int flags;	/* driver attachment info */
	int instance;	/* next instance to assign */
	uint_t ops;	/* bit-encoded driver ops */
};

struct di_node {	/* useful info to export for each tree node */
	/*
	 * offset to di_node structures
	 */
	di_off_t self;		/* make it self addressable */
	di_off_t parent;	/* offset of parent node */
	di_off_t child;		/* offset of child node */
	di_off_t sibling;	/* offset of sibling */
	di_off_t next;		/* next node on per-instance list */
	/*
	 * offset to char strings of current node
	 */
	di_off_t node_name;	/* offset of device node name */
	di_off_t address;	/* offset of address part of name */
	di_off_t bind_name;	/* offset of binding name */
	di_off_t compat_names;	/* offset of compatible names */
	/*
	 * offset to property lists, private data, etc.
	 */
	di_off_t minor_data;
	di_off_t drv_prop;
	di_off_t sys_prop;
	di_off_t hw_prop;
	di_off_t parent_data;
	di_off_t driver_data;
	di_off_t devid;		/* registered device id */
	di_off_t pm_info;	/* RESERVED FOR FUTURE USE */
	/*
	 * misc values
	 */
	int compat_length;	/* size of compatible name list */
	int drv_major;		/* for indexing into devnames array */
	/*
	 * value attributes of current node
	 */
	int instance;		/* instance number */
	int nodeid;		/* node id */
	ddi_node_class_t node_class;	/* node class */
	int attributes;		/* node attributes */
	uint_t state;		/* hotplugging device state */
	uint_t node_state;	/* 0 = detached, 1 = attached */
};

/*
 * chain of ddi_minor_data structure
 */
struct di_minor {
	di_off_t	self;		/* make it self addressable */
	di_off_t	next;		/* next one in the chain */
	di_off_t	name;		/* name of node */
	di_off_t	node_type;	/* block, byte, serial, network */
	ddi_minor_type	type;		/* data type */
	major_t		dev_major;	/* dev_t can be 64-bit */
	minor_t		dev_minor;
	int		spec_type;	/* block or char */
	unsigned int	mdclass;	/* minor device class */
};

/*
 * Now the properties.
 */
struct di_prop {
	di_off_t	self;		/* make it self addressable */
	di_off_t	next;
	di_off_t	prop_name;	/* Property name */
	di_off_t	prop_data;	/* property data */
	major_t		dev_major;	/* dev_t can be 64 bit */
	minor_t		dev_minor;
	int	prop_flags;	/* mark prop value types & more */
	int	prop_len;	/* prop length in bytes (boolean if 0) */
	int	prop_list;	/* which list (DI_PROP_SYS_LIST), etc */
};

/*
 * Private data stuff for supporting prtconf.
 * Allows one level of indirection of fixed sized obj or obj array.
 * The array size may be an int member of the array.
 */

struct di_priv_format {
	char drv_name[MAXPATHLEN];	/* name of parent drv for ppdata */
	size_t bytes;			/* size in bytes of this struct */
	struct {			/* ptrs to dereference */
		int size;	/* size of object assoc. this ptr */
		int offset;	/* location of pointer within struct */
		int len_offset;	/* offset to var. containing the len */
	} ptr[MAX_PTR_IN_PRV];
};

struct di_priv_data {
	int version;
	int n_parent;
	int n_driver;
	struct di_priv_format *parent;
	struct di_priv_format *driver;
};

/*
 * structure passed in from ioctl
 */
struct dinfo_io {
	char root_path[MAXPATHLEN];
	struct di_priv_data priv;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DEVINFO_IMPL_H */
