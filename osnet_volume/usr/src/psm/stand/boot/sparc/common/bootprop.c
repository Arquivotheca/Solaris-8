/*
 * Copyright (c) 1991-1999 Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bootprop.c	1.21	99/02/23 SMI"

#include <sys/types.h>
#include <sys/promif.h>
#include <sys/bootconf.h>
#include <sys/salib.h>

extern struct memlist *vfreelistp, *pfreelistp, *pinstalledp;
extern char *v2path, *v2args, *kernname, *systype, *my_own_name;
extern char *mfg_name;
extern char *impl_arch_name;
char *bootp_response;
extern char *module_path;
extern char *backfs_dev, *backfs_fstype;
extern char *frontfs_dev, *frontfs_fstype;
extern int  vac;
extern int  cache_state;
extern uint64_t memlistextent;
#ifdef	__sparcv9
extern char *warn_about_default;
#endif
char *boot_message;

#ifdef PROP_DEBUG
static int debug = 1;
#else PROP_DEBUG
#define	debug	0
#endif PROP_DEBUG

#define	dprintf		if (debug) printf

/*
 *  Exported prototypes
 */
extern	int	bgetprop(struct bootops *, char *name, void *buf);
extern	int	bgetproplen(struct bootops *, char *name);
extern	char	*bnextprop(struct bootops *, char *name);

extern	void	update_memlist(char *, char *, struct memlist **);

/*
 * Support new boot properties "boot-start" and "boot-end" for
 * Freeze/Thaw project.
 */
extern	void	_start(void *romp, ...);
extern	caddr_t	scratchmemp;
caddr_t start_addr, end_addr;

#define	BOOT_BADPROP	-1
#define	BOOT_SUCCESS	0
#define	BOOT_FAILURE	-1
#define	NIL		0

#define	strequal(p, q)	(strcmp((p), (q)) == 0)

static const struct bplist {
	char	*name;
	void	*val;
	u_int	size;
} bprop_tab[] = {

	"boot-args",		&v2args,		0,
	"boot-path",		&v2path,		0,
	"fstype",		&systype,		0,
	"whoami",		&my_own_name,		0,
	"mfg-name",		&mfg_name,		0,
	"impl-arch-name",	&impl_arch_name,	0,
	"module-path", 		&module_path,		0,
	"virt-avail",		&vfreelistp,		0,
	"phys-avail",		&pfreelistp,		0,
	"phys-installed",	&pinstalledp,		0,
	"default-name",		&kernname,		0,
	"extent",		&memlistextent,		sizeof (memlistextent),
	"vac",			&vac,			sizeof (vac),
	"cache-on?",		&cache_state,		sizeof (int),
	"memory-update",	0,			0,
	"boot-start",		&start_addr,		sizeof (start_addr),
	"boot-end",		&scratchmemp,		sizeof (scratchmemp),
	"backfs-path",		&backfs_dev,		0,
	"backfs-fstype",	&backfs_fstype,		0,
	"frontfs-path",		&frontfs_dev,		0,
	"frontfs-fstype",	&frontfs_fstype,	0,
	"boot-message",		&boot_message,		0,
#ifdef	__sparcv9
	"warn-about-default",	&warn_about_default,	0,
#endif
	"bootp-response",	&bootp_response,	0,
	0,			0,			0
};

/*
 *  These routines implement the boot getprop interface.
 *  They are designed to mimic the corresponding devr_{getprop,getproplen}
 *  functions.
 *  The assumptions is that the basic property is an unsigned int.  Other
 *  types (including lists) are special cases.
 */

/*ARGSUSED*/
int
bgetproplen(struct bootops *bop, char *name)
{
	int size = 0;
	struct bplist *p;
	struct memlist *ml;

	/* this prop has side effects only.  No length.  */
	if (strequal(name, "memory-update"))
		return (BOOT_SUCCESS);

	for (p = (struct bplist *)bprop_tab; p->name != (char *)0; p++) {

		/* got a linked list?  */
		if ((strequal(name, "virt-avail") && strequal(name, p->name)) ||
		    (strequal(name, "phys-avail") && strequal(name, p->name)) ||
		    (strequal(name, "phys-installed") &&
		    strequal(name, p->name))) {

			for (ml = *((struct memlist **)p->val);
					ml != NIL;
					ml = ml->next)

				/*
				 *  subtract out the ptrs for our local
				 *  linked list.  The application will
				 *  only see an array.
				 */
				size += (sizeof (struct memlist) -
						2*sizeof (struct memlist *));
			return (size);

		} else if (strequal(name, p->name)) {

			/* if we already know the size, return it */
			if (p->size != 0)
				return (p->size);
			else {
				if (*((char **)p->val) == NIL)
					return (0);	/* NULL is allowed */

				/* don't forget the null termination */
				return (strlen(*((char **)p->val)) + 1);
			}
		}
	}
	printf("Property (%s) not supported by %s\n", name, my_own_name);
	return (BOOT_BADPROP);
}

/*ARGSUSED*/
int
bgetprop(struct bootops *bop, char *name, void *buf)
{
	struct bplist *p;
	struct memlist *ml;

	if (strequal(name, "memory-update")) {
/*
		dprintf("bgetprop:  updating memlists.\n");
*/
		update_memlist("virtual-memory", "available", &vfreelistp);
		update_memlist("memory", "available", &pfreelistp);
		return (BOOT_SUCCESS);
	}

	if (strequal(name, "boot-start")) {
		start_addr = (caddr_t)_start;
		bcopy((char *)(&start_addr), buf, sizeof (start_addr));
		return (BOOT_SUCCESS);
	}

	if (strequal(name, "boot-end")) {
		/*
		 * The true end of boot should be scratchmemp,
		 * boot gets its dynamic memory from the scratchmem
		 * which is the first 4M of the physical memory,
		 * and they are mapped 1:1.
		 */
		end_addr = scratchmemp;
		bcopy((char *)(&end_addr), buf, sizeof (scratchmemp));
		return (BOOT_SUCCESS);
	}

	for (p = (struct bplist *)bprop_tab; p->name != (char *)0; p++) {

		/* gotta linked list? */
		if ((strequal(name, "virt-avail") && strequal(name, p->name)) ||
		    (strequal(name, "phys-avail") && strequal(name, p->name)) ||
		    (strequal(name, "phys-installed") &&
		    strequal(name, p->name))) {

			u_longlong_t *t = buf;

			for (ml = *((struct memlist **)p->val);
					ml != NIL;
					ml = ml->next) {

				/* copy out into an array */
				*t++ = ml->address;
				*t++ = ml->size;
			}
			return (BOOT_SUCCESS);
		} else if (strequal(name, p->name)) {
			if (p->size != 0) {
				bcopy(p->val, buf, p->size);
			} else {
				(void) strcpy((char *)buf, *((char **)p->val));
			}
			return (BOOT_SUCCESS);
		}
	}
	return (BOOT_FAILURE);
}

/*
 *  If the user wants the first property in the list, he passes in a
 *  null string.  The routine will always return a ptr to the name of the
 *  next prop, except when there are no more props.  In that case, it will
 *  return a null string.
 */

/*ARGSUSED*/
char *
bnextprop(struct bootops *bop, char *prev)
{
	struct bplist *p;

	/* user wants the firstprop */
	if (*prev == 0)
		return (bprop_tab->name);

	for (p = (struct bplist *)bprop_tab; p->name != (char *)0; p++) {

		if (strequal(prev, p->name))
			/*
			 * if prev is the last valid prop,
			 * we will return our terminator (0).
			 */
			return ((++p)->name);


	}
	return ((char *)0);
}
