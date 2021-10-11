/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_BOOTCONF_H
#define	_SYS_BOOTCONF_H

#pragma ident	"@(#)bootconf.h	1.32	99/05/04 SMI"

/*
 * Boot time configuration information objects
 */

#include <sys/types.h>
#include <sys/reg.h>		/* for struct bop_regs */
#include <sys/stat.h>		/* for struct stat */
#include <sys/dirent.h>		/* for struct dirent */
#include <sys/memlist.h>
#include <sys/obpdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * masks to hand to bsys_alloc memory allocator
 * XXX	These names shouldn't really be srmmu derived.
 */
#define	BO_NO_ALIGN	0x00001000

/* flags for BOP_EALLOC */
#define	BOPF_X86_ALLOC_CLIENT	0x001
#define	BOPF_X86_ALLOC_REAL	0x002

/* return values for the newer bootops */
#define	BOOT_SUCCESS	0
#define	BOOT_FAILURE	(-1)

/*
 *  We pass a ptr to the space that boot has been using
 *  for its memory lists.
 */
struct bsys_mem {
	struct memlist *physinstalled;	/* amt of physmem installed */
	struct memlist *physavail;	/* amt of physmem avail for use */
	struct memlist *virtavail;	/* amt of virtmem avail for use */
	uint_t		extent; 	/* number of bytes in the space */
};

/*
 * Warning: Changing BO_VERSION blows compatibility between booters
 *          and older kernels.  If you want to change the struct bootops,
 *          please consider adding new stuff to the end and using the
 *          "bootops-extensions" mechanism described below.
 */
#define	BO_VERSION	5		/* bootops interface revision # */

struct bootops {
	/*
	 * the ubiquitous version number
	 */
	uint_t	bsys_version;

	/*
	 * pointer to our parents bootops
	 */
	struct bootops	*bsys_super;

	/*
	 * the area containing boot's memlists
	 */
	struct 	bsys_mem *boot_mem;

	/*
	 * open a file
	 */
	int	(*bsys_open)(struct bootops *, char *s, int flags);

	/*
	 * read from a file
	 */
	int	(*bsys_read)(struct bootops *, int fd, caddr_t buf,
	    size_t size);

	/*
	 * seek (hi<<32) + lo bytes into a file
	 */
	int	(*bsys_seek)(struct bootops *, int fd, off_t hi, off_t lo);

	/*
	 * for completeness..
	 */
	int	(*bsys_close)(struct bootops *, int fd);

	/*
	 * have boot allocate size bytes at virthint
	 */
	caddr_t	(*bsys_alloc)(struct bootops *, caddr_t virthint, size_t size,
		int align);

	/*
	 * free size bytes allocated at virt - put the
	 * address range back onto the avail lists.
	 */
	void	(*bsys_free)(struct bootops *, caddr_t virt, size_t size);

	/*
	 * associate a physical mapping with the given vaddr
	 */
	caddr_t	(*bsys_map)(struct bootops *, caddr_t virt, int space,
	    caddr_t phys, size_t size);

	/*
	 * disassociate the mapping with the given vaddr
	 */
	void	(*bsys_unmap)(struct bootops *, caddr_t virt, size_t size);

	/*
	 * boot should quiesce its io resources
	 */
	void	(*bsys_quiesce_io)(struct bootops *);

	/*
	 * to find the size of the buffer to allocate
	 */
	int	(*bsys_getproplen)(struct bootops *, char *name);

	/*
	 * get the value associated with this name
	 */
	int	(*bsys_getprop)(struct bootops *, char *name, void *value);

	/*
	 * set the value associated with this name
	 */
	int	(*bsys_setprop)(struct bootops *, char *name, char *value);

	/*
	 * get the name of the next property in succession
	 * from the standalone
	 */
	char	*(*bsys_nextprop)(struct bootops *, char *prevprop);

	/*
	 * print formatted output
	 */
	void	(*bsys_printf)(struct bootops *, char *, ...);

	/*
	 * Do a real mode interrupt
	 */
	void	(*bsys_doint)(struct bootops *, int, struct bop_regs *);

	/*
	 * End of bootops required for BO_VERSION == 5.  Do not change anything
	 * above here without bumping BO_VERSION.
	 *
	 * Before using the bootops below, the caller must perform a run-time
	 * check to see if the desired bootops enhancements are supported by
	 * the boot.  This is done by looking at the value of the property
	 * "bootops-extensions".  Unlike BO_VERSION, which must match
	 * exactly, the "bootops-extensions" property describes which sets
	 * bootops have been added to the end of this struct WITHOUT CHANGING
	 * ANYTHING ELSE.  If the caller wants to check for the Nth set
	 * of extensions, the check is:
	 *
	 *	if (the value of "bootops-extensions" >= N)
	 *		the Nth set of extensions exists
	 *	else
	 *		must make do without those extensions
	 */

	/* bootops which exist if (bootops-extensions >= 1) ... */

	/*
	 * 1275-ish version of getproplen (takes a phandle)
	 */
	int (*bsys1275_getproplen)(struct bootops *, char *, phandle_t);

	/*
	 * 1275-ish version of getprop (takes a phandle)
	 */
	int (*bsys1275_getprop)(struct bootops *,
	    char *, caddr_t, int, phandle_t);

	/*
	 * 1275-ish version of setprop (takes a phandle)
	 */
	int (*bsys1275_setprop)(struct bootops *,
	    char *, caddr_t, int, phandle_t);

	/*
	 * 1275-ish version of nextprop (takes a phandle)
	 */
	int (*bsys1275_nextprop)(struct bootops *, char *, char *, phandle_t);

	/*
	 * mount a filesystem
	 */
	int (*bsys_mount)(struct bootops *, char *, char *, char *);

	/*
	 * umount a filesystem
	 */
	int (*bsys_umount)(struct bootops *, char *);

	/*
	 * get file attributes
	 */
	int (*bsys_fstat)(struct bootops *, int, struct stat *);

	/*
	 * get directory entries
	 */
	int (*bsys_getdents)(struct bootops *, int, struct dirent *, size_t);

	/*
	 * 1275-ish "peer" function
	 */
	phandle_t (*bsys1275_peer)(struct bootops *, phandle_t);

	/*
	 * 1275-ish "child" function
	 */
	phandle_t (*bsys1275_child)(struct bootops *, phandle_t);

	/*
	 * 1275-ish "parent" function
	 */
	phandle_t (*bsys1275_parent)(struct bootops *, phandle_t);

	/*
	 * 1275-ish "self" function
	 */
	ihandle_t (*bsys1275_self)(struct bootops *);

	/*
	 * 1275-ish "inst2path" function
	 */
	int (*bsys1275_inst2path)(struct bootops *, ihandle_t, char *, int);

	/*
	 * 1275-ish "inst2pkg" function
	 */
	phandle_t (*bsys1275_inst2pkg)(struct bootops *, ihandle_t);

	/*
	 * 1275-ish "pkg2path" function
	 */
	int (*bsys1275_pkg2path)(struct bootops *, phandle_t, char *, int);

	/*
	 * Construct a node in the device tree
	 */
	phandle_t (*bsys1275_mknod)(struct bootops *, phandle_t);

	/*
	 * Run a real-mode program under the second level boot
	 */
	int (*bsys_exec_real)(struct bootops *, int, char **);

	/*
	 * Call real-mode code via software interrupt or jump
	 * (i.e. enhanced version of bsys_doint()).
	 */
	int (*bsys_call_real)(struct bootops *, caddr_t, struct bop_regs *);

	/*
	 * Convert between client's address space and real-mode's
	 */
	long (*bsys_cvt_addr)(struct bootops *, unsigned long, int);

	/*
	 * Enhanced version of bsys_alloc().
	 */
	caddr_t	(*bsys_ealloc)(struct bootops *, caddr_t virthint, size_t size,
		int align, int flags);

	/* end of bootops which exist if (bootops-extensions >= 1) */
};

#define	BOP_GETVERSION(bop)		((bop)->bsys_version)
#define	BOP_OPEN(bop, s, flags)		((bop)->bsys_open)(bop, s, flags)
#define	BOP_READ(bop, fd, buf, size)	((bop)->bsys_read)(bop, fd, buf, size)
#define	BOP_SEEK(bop, fd, hi, lo)	((bop)->bsys_seek)(bop, fd, hi, lo)
#define	BOP_CLOSE(bop, fd)		((bop)->bsys_close)(bop, fd)
#define	BOP_ALLOC(bop, virthint, size, align)	\
				((bop)->bsys_alloc)(bop, virthint, size, align)
#define	BOP_FREE(bop, virt, size)	((bop)->bsys_free)(bop, virt, size)
#define	BOP_MAP(bop, virt, space, phys, size)	\
				((bop)->bsys_map)(bop, virt, space, phys, size)
#define	BOP_UNMAP(bop, virt, size)	((bop)->bsys_unmap)(bop, virt, size)
#define	BOP_QUIESCE_IO(bop)		((bop)->bsys_quiesce_io)(bop)
#define	BOP_GETPROPLEN(bop, name)	((bop)->bsys_getproplen)(bop, name)
#define	BOP_GETPROP(bop, name, buf)	((bop)->bsys_getprop)(bop, name, buf)
#define	BOP_SETPROP(bop, name, buf)	((bop)->bsys_setprop)(bop, name, buf)
#define	BOP_NEXTPROP(bop, prev)		((bop)->bsys_nextprop)(bop, prev)
#define	BOP_DOINT(bop, intnum, rp)	((bop)->bsys_doint)(bop, intnum, rp)

/* bootops which exist if (bootops-extensions >= 1) */
#define	BOP1275_GETPROPLEN(bop, node, name)\
		((bop)->bsys1275_getproplen)(bop, name, node)
#define	BOP1275_GETPROP(bop, node, name, buf, len)\
		((bop)->bsys1275_getprop)(bop, name, buf, len, node)
#define	BOP1275_SETPROP(bop, node, name, buf, len)\
		((bop)->bsys1275_setprop)(bop, name, buf, len, node)
#define	BOP1275_NEXTPROP(bop, node, prev, buf)\
		((bop)->bsys1275_nextprop)(bop, prev, buf, node)
#define	BOP_MOUNT(bop, dev, mpt, type) ((bop)->bsys_mount)(bop, dev, mpt, type)
#define	BOP_UNMOUNT(bop, mpt) ((bop)->bsys_umount)(bop, mpt)
#define	BOP_GETATTR(bop, fd, buf) ((bop)->bsys_fstat)(bop, fd, buf)
#define	BOP_GETDENTS(bop, fd, buf, size)\
		((bop)->bsys_getdents)(bop, fd, buf, size)
#define	BOP1275_PEER(bop, node) ((bop)->bsys1275_peer)(bop, node)
#define	BOP1275_CHILD(bop, node) ((bop)->bsys1275_child)(bop, node)
#define	BOP1275_PARENT(bop, node) ((bop)->bsys1275_parent)(bop, node)
#define	BOP1275_MY_SELF(bop) ((bop)->bsys1275_self)(bop);
#define	BOP1275_INSTANCE_TO_PATH(bop, dev, path, len)\
		((bop)->bsys1275_inst2path)(bop, dev, path, len)
#define	BOP1275_INSTANCE_TO_PACKAGE(bop, dev)\
		((bop)->bsys1275_inst2pkg)(bop, dev)
#define	BOP1275_PACKAGE_TO_PATH(bop, node, path, len)\
		((bop)->bsys1275_pkg2path)(bop, node, path, len)
#define	BOP1275_CREATE_NODE(bop, parent) ((bop)->bsys1275_mknod)(bop, parent)
#define	BOP_RUN(bop, argc, argv) ((bop)->bsys_exec_real)(bop, argc, argv)
#define	BOP_DOFAR(bop, addr, regs) ((bop)->bsys_call_real)(bop, addr, regs)
#define	BOP_REAL_ADDR(bop, lin) ((bop)->bsys_cvt_addr)(bop, (long)lin, 0)
#define	BOP_PROT_ADDR(bop, rel)\
		(caddr_t)((bop)->bsys_cvt_addr)(bop, (long)rel, 1)
#define	BOP_EALLOC(bop, virthint, size, align, flags)\
		((bop)->bsys_ealloc)(bop, virthint, size, align, flags)
/* end of bootops which exist if (bootops-extensions >= 1) */

#if defined(_KERNEL) && !defined(_BOOT)

/*
 * Boot configuration information
 */

#define	BO_MAXFSNAME	16
#define	BO_MAXOBJNAME	256

struct bootobj {
	char	bo_fstype[BO_MAXFSNAME];	/* vfs type name (e.g. nfs) */
	char	bo_name[BO_MAXOBJNAME];		/* name of object */
	int	bo_flags;			/* flags, see below */
	int	bo_size;			/* number of blocks */
	struct vnode *bo_vp;			/* vnode of object */
};

/*
 * flags
 */
#define	BO_VALID	0x01	/* all information in object is valid */
#define	BO_BUSY		0x02	/* object is busy */

extern struct bootobj rootfs;
extern struct bootobj frontfs;
extern struct bootobj backfs;
extern struct bootobj swapfile;

extern dev_t getrootdev(void);
extern dev_t getswapdev(char *);
extern void getfsname(char *, char *);
extern int loadrootmodules(void);
extern int loaddrv_hierarchy(char *, major_t);

extern void strplumb(void);
extern int strplumb_get_driver_list(int, char **, char **);

extern void consconfig(void);
extern void release_bootstrap(void);

extern void param_check(void);

/*
 * XXX	The memlist stuff belongs in a header of its own
 */
extern int check_boot_version(int);
extern void size_physavail(struct memlist *, pgcnt_t *, int *, pfn_t);
extern int copy_physavail(struct memlist *, struct memlist **,
    uint_t, uint_t);
extern void installed_top_size(struct memlist *, pfn_t *, pgcnt_t *, pfn_t);
extern int check_memexp(struct memlist *, uint_t);
extern void copy_memlist(struct memlist *, struct memlist **);

extern struct bootops *bootops;
extern int netboot;
extern int swaploaded;
extern int modrootloaded;
extern char kern_bootargs[];
extern char *default_path;
extern char *dhcack;
extern int dhcacklen;

#endif /* _KERNEL && !_BOOT */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_BOOTCONF_H */
