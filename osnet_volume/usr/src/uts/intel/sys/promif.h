/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PROMIF_H
#define	_SYS_PROMIF_H

#pragma ident	"@(#)promif.h	1.20	99/06/06 SMI"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/obpdefs.h>
#include <sys/dditypes.h>
#include <sys/ddidmareq.h>
#include <sys/ddi_impldefs.h>
#include <sys/archsystm.h>
#include <sys/varargs.h>

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

extern	caddr_t		prom_map(caddr_t virthint, uint_t space,
			    uint_t phys, uint_t size);

/*
 * resource allocation group: OBP and IEEE 1275-1994.
 * prom_alloc is platform dependent on SPARC.
 */
extern	caddr_t		prom_alloc(caddr_t virthint, uint_t size, int align);
extern	void		prom_free(caddr_t virt, uint_t size);

/*
 * Device tree and property group: OBP and IEEE 1275-1994.
 */
extern	dnode_t		prom_childnode(dnode_t nodeid);
extern	dnode_t		prom_nextnode(dnode_t nodeid);
extern	dnode_t		prom_optionsnode(void);
extern	dnode_t		prom_alias_node(void);
extern	dnode_t		prom_rootnode(void);

extern	int		prom_getproplen(dnode_t nodeid, caddr_t name);
extern	int		prom_getprop(dnode_t nodeid, caddr_t name,
			    caddr_t value);
extern	caddr_t		prom_nextprop(dnode_t nodeid, caddr_t previous,
			    caddr_t next);

extern	char		*prom_decode_composite_string(void *buf,
			    size_t buflen, char *prev);

/*
 * Device tree and property group: IEEE 1275-1994 Only.
 */
extern	dnode_t		prom_finddevice(char *path);

extern	int		prom_bounded_getprop(dnode_t nodeid,
			    caddr_t name, caddr_t buffer, int buflen);

/*
 * Device pathnames and pathname conversion: OBP and IEEE 1275-1994.
 */
extern	int		prom_devname_from_pathname(char *path, char *buffer);
extern	char		*prom_path_gettoken(char *from, char *to);

/*
 * Device pathnames and pathname conversion: IEEE 1275-1994 only.
 */

/*
 * Special device nodes: OBP and IEEE 1275-1994.
 */
extern	int		prom_stdin_is_keyboard(void);
extern	int		prom_stdout_is_framebuffer(void);

/*
 * Special device nodes: IEEE 1275-1994 only.
 */

/*
 * Administrative group: OBP and IEEE 1275-1994.
 */
extern	void		prom_enter_mon(void);
extern	void		prom_exit_to_mon(void);
extern	void		prom_reboot(char *bootstr);

extern	void		prom_panic(char *string);

extern	int		prom_is_openprom(void);
extern	int		prom_is_p1275(void);
extern	int		prom_version_name(char *buf, int buflen);

extern	uint_t		prom_gettime(void);

extern	char		*prom_bootpath(void);
extern	char		*prom_bootargs(void);

/*
 * Administrative group: OBP only.
 */

/*
 * Administrative group: IEEE 1275-1994 only.
 */

/*
 * Administrative group: IEEE 1275 only.
 */

/*
 * Promif support group: Generic.
 */
extern	void		prom_init(char *progname, void *prom_cookie);

/*
 * I/O Group: OBP and IEEE 1275.
 */
extern	uchar_t		prom_getchar(void);
extern	void		prom_putchar(char c);
extern	int		prom_mayget(void);
extern	int		prom_mayput(char c);

extern  int		prom_open(char *name);
extern  int		prom_close(int fd);
extern  int		prom_read(int fd, caddr_t buf, uint_t len,
			    uint_t startblk, char type);
extern	int		prom_write(int fd, caddr_t buf, uint_t len,
			    uint_t startblk, char devtype);
extern	int		prom_seek(int fd, unsigned long long offset);

/*PRINTFLIKE1*/
extern	void		prom_printf(const char *fmt, ...);
extern	void		prom_vprintf(const char *fmt, va_list adx);
/*PRINTFLIKE2*/
extern	char		*prom_sprintf(char *s, const char *fmt, ...);
extern	char		*prom_vsprintf(char *s, const char *fmt, va_list adx);
/*
 * promif tree searching routines ... OBP and IEEE 1275-1994.
 */

extern	dnode_t		prom_findnode_byname(dnode_t id, char *name);

#define	PROM_STOP	{	\
	prom_printf("File %s line %d\n", __FILE__, __LINE__); \
	prom_enter_mon();	\
}


#endif	/* _KERNEL */
#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROMIF_H */
