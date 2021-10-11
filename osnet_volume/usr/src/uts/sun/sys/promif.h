/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PROMIF_H
#define	_SYS_PROMIF_H

#pragma ident	"@(#)promif.h	1.43	99/05/05 SMI"

#include <sys/types.h>
#include <sys/obpdefs.h>
#ifdef _KERNEL
#include <sys/va_list.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *  These are for V0 ops only.  We sometimes have to specify
 *  to promif which type of operation we need to perform
 *  and since we can't get such a property from a V0 prom, we
 *  sometimes just assume it.  V2 and later proms do the right thing.
 */
#define	BLOCK	0
#define	NETWORK	1
#define	BYTE	2


#ifdef	_KERNEL

#if defined(__sparcv9) && !defined(_BOOT)
/*
 * Due to FCode on sun4u machines running in a pseudo-32-bit environment
 * we need to enable code in several of the promif routines to ensure
 * that 64-bit pointers from the kernel are not passed through the CIF
 * to OpenBoot.
 *
 * Client programs defining this token need to provide two callbacks to
 * allow the promif routines to allocate and free memory allocated from
 * the bottom 32-bits of the 64-bit address space:
 *
 * 	void *promplat_alloc(size_t);
 * 	void promplat_free(void *, size_t);
 *
 * The alloc function should guarantee that it will never return an
 * invalid pointer.
 */
#define	PROM_32BIT_ADDRS
#endif /* __sparcv9 */

/*
 * resource allocation group: OBP and IEEE 1275-1994.
 * prom_alloc is platform dependent on SPARC.
 */
extern	caddr_t		prom_alloc(caddr_t virthint, size_t size, uint_t align);
extern	void		prom_free(caddr_t virt, size_t size);

/*
 * Device tree and property group: OBP and IEEE 1275-1994.
 */
extern	dnode_t		prom_childnode(dnode_t nodeid);
extern	dnode_t		prom_nextnode(dnode_t nodeid);
extern	dnode_t		prom_parentnode(dnode_t nodeid);
extern	dnode_t		prom_rootnode(void);
extern	dnode_t		prom_chosennode(void);
extern	dnode_t		prom_alias_node(void);
extern	dnode_t		prom_optionsnode(void);

extern	int		prom_getproplen(dnode_t nodeid, caddr_t name);
extern	int		prom_getprop(dnode_t nodeid, caddr_t name,
			    caddr_t value);
extern	caddr_t		prom_nextprop(dnode_t nodeid, caddr_t previous,
			    caddr_t next);
extern	int		prom_setprop(dnode_t nodeid, caddr_t name,
			    caddr_t value, int len);

extern	int		prom_getnode_byname(dnode_t id, char *name);
extern	int		prom_devicetype(dnode_t id, char *type);

extern	char		*prom_decode_composite_string(void *buf,
			    size_t buflen, char *prev);

/*
 * Device tree and property group: IEEE 1275-1994 Only.
 */
extern	dnode_t		prom_finddevice(char *path);	/* Also on obp2.x */

extern	int		prom_bounded_getprop(dnode_t nodeid,
			    caddr_t name, caddr_t buffer, int buflen);

extern	phandle_t	prom_getphandle(ihandle_t i);

/*
 * Device pathnames and pathname conversion: OBP and IEEE 1275-1994.
 */
extern	int		prom_devname_from_pathname(char *path, char *buffer);
extern	char		*prom_path_options(char *pathname);
extern	char		*prom_path_gettoken(char *from, char *to);
extern	void		prom_pathname(char *pathname);
extern	void		prom_strip_options(char *from, char *to);

/*
 * Device pathnames and pathname conversion: IEEE 1275-1994 only.
 */
extern	int		prom_ihandle_to_path(ihandle_t, char *buf,
			    uint_t buflen);
extern	int		prom_phandle_to_path(phandle_t, char *buf,
			    uint_t buflen);

/*
 * Special device nodes: OBP and IEEE 1275-1994.
 */
extern	ihandle_t	prom_stdin_ihandle(void);
extern	ihandle_t	prom_stdout_ihandle(void);
extern	dnode_t		prom_stdin_node(void);
extern	dnode_t		prom_stdout_node(void);
extern	char		*prom_stdinpath(void);
extern	char		*prom_stdoutpath(void);
extern	int		prom_stdin_devname(char *buffer);
extern	int		prom_stdout_devname(char *buffer);
extern	int		prom_stdin_is_keyboard(void);
extern	int		prom_stdout_is_framebuffer(void);
extern	int		prom_stdin_stdout_equivalence(void);

/*
 * Special device nodes: IEEE 1275-1994 only.
 */
extern	ihandle_t	prom_memory_ihandle(void);
extern	ihandle_t	prom_mmu_ihandle(void);

/*
 * Administrative group: OBP and IEEE 1275-1994.
 */
extern	void		prom_enter_mon(void);
extern	void		prom_exit_to_mon(void);
extern	void		prom_reboot(char *bootstr);

extern	void		prom_panic(char *string);

extern	int		prom_getversion(void);
extern	int		prom_is_openprom(void);
extern	int		prom_is_p1275(void);
extern	int		prom_version_name(char *buf, int buflen);
extern	int		prom_version_check(char *buf, size_t len, dnode_t *n);

extern	void		*prom_mon_id(void);	/* SMCC/OBP platform centric */

extern	uint_t		prom_gettime(void);

extern	char		*prom_bootpath(void);
extern	char		*prom_bootargs(void);

extern	void		prom_interpret(char *str, uintptr_t arg1,
			    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4,
			    uintptr_t arg5);

/*
 * Return code values from prom_version_check:
 *
 * This routine uses past-prediction mode to determine if the firmware
 * on the current system is 64-bit ready.
 *
 * return code 2 could happen on a board-based server with a slave CPU board
 * running down-rev firmware and the current master running adequate fw.
 */
#define	PROM_VER64_OK		0	/* Prom is 64-bit ready (or n/a) */
#define	PROM_VER64_UPGRADE	1	/* Down-rev firmware is running */
#define	PROM_VER64_SUGGEST	2	/* Down-rev firmware detected .. */
					/* .. but not currently active */

/*
 * Administrative group: OBP only.
 */
extern	int		prom_sethandler(void (*v0_func)(), void (*v2_func)());

extern	struct bootparam *prom_bootparam(void);

/*
 * Administrative group: IEEE 1275-1994 only.
 */
extern void		*prom_set_callback(void *handler);
extern void		prom_set_symbol_lookup(void *sym2val, void *val2sym);

/*
 * Administrative group: IEEE 1275 only.
 */
extern	int		prom_test(char *service);
extern	int		prom_test_method(char *method, dnode_t node);

/*
 * Promif support group: Generic.
 */
extern	void		prom_init(char *progname, void *prom_cookie);

extern	void		(*prom_set_preprom(void (*)(void)))(void);
extern	void		(*prom_set_postprom(void (*)(void)))(void);

extern	void		(*prom_set_prop_preprom(void (*)(void)))(void);
extern	void		(*prom_set_prop_postprom(void (*)(void)))(void);

extern	void		prom_montrap(void (*funcptr)());

/*
 * I/O Group: OBP and IEEE 1275.
 */
extern	uchar_t		prom_getchar(void);
extern	void		prom_putchar(char c);
extern	int		prom_mayget(void);
extern	int		prom_mayput(char c);

extern  int		prom_open(char *name);
extern  int		prom_close(int fd);
extern  ssize_t		prom_read(ihandle_t fd, caddr_t buf, size_t len,
			    uint_t startblk, char type);
extern  ssize_t		prom_write(ihandle_t fd, caddr_t buf, size_t len,
			    uint_t startblk, char type);
extern	int		prom_seek(int fd, u_longlong_t offset);

extern	void		prom_writestr(const char *buf, size_t bufsize);
extern	void		prom_dnode_to_pathname(dnode_t, char *);

/*PRINTFLIKE1*/
extern	void		prom_printf(const char *fmt, ...);
extern	void		prom_vprintf(const char *fmt, __va_list adx);
/*PRINTFLIKE2*/
extern	char		*prom_sprintf(char *s, const char *fmt, ...);
extern	char		*prom_vsprintf(char *s, const char *fmt, __va_list adx);

/*
 * promif tree searching routines ... OBP and IEEE 1275-1994.
 */
typedef struct prom_stack {
	dnode_t *sp;
	dnode_t *minstack;
	dnode_t *maxstack;
} pstack_t;

extern	pstack_t	*prom_stack_init(dnode_t *buf, size_t max);
extern	void		prom_stack_fini(pstack_t *ps);

extern	dnode_t		prom_findnode_byname(dnode_t id, char *name,
			    pstack_t *ps);
extern	dnode_t		prom_findnode_bydevtype(dnode_t id, char *devtype,
			    pstack_t *ps);


#define	PROM_STOP	{	\
	prom_printf("File %s line %d\n", __FILE__, __LINE__); \
	prom_enter_mon();	\
}


#endif	/* _KERNEL */
#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROMIF_H */
