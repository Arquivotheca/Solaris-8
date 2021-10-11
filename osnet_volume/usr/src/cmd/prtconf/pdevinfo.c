/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pdevinfo.c	1.45	99/10/15 SMI"

/*
 * For machines that support the openprom, fetch and print the list
 * of devices that the kernel has fetched from the prom or conjured up.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <strings.h>
#include <unistd.h>
#include <stropts.h>
#include <sys/sunddi.h>
#include <sys/openpromio.h>
#include <sys/modctl.h>
#include "prtconf.h"

static void walk_driver(di_node_t);
static int dump_devs(di_node_t, void *);
static int dump_prop_list(char *, int, di_node_t,
    di_prop_t (*nxtprop)(di_node_t, di_prop_t));
static int _error(const char *, ...);
static int is_openprom();
static void walk(int, int);
static void dump_node(int, int);
static int print_composite_string(const char *, struct openpromio *);
static void print_one(const char *, int);
static int unprintable(struct openpromio *);
static int promopen(int);
static void promclose();
static int getpropval(struct openpromio *);
static int next(int);
static int child(int);

extern int modctl(int, ...);

void
prtconf_devinfo(void)
{
	struct di_priv_data fetch;
	di_node_t root_node;
	uint_t flag;

	dprintf("verbosemode %s\n", opts.o_verbose ? "on" : "off");

	/* determine what info we need to get from kernel */
	flag = DINFOSUBTREE;

	if (opts.o_verbose) {
		flag |= (DINFOPROP | DINFOPRIVDATA);
		if (dbg.d_forceload) {
			flag |= DINFOFORCE;
		}
		init_priv_data(&fetch);
		root_node = di_init_impl("/", flag, &fetch);
	} else
		root_node = di_init("/", flag);

	if (root_node == DI_NODE_NIL)
		exit(_error("di_init() failed."));

	/*
	 * ...and walk all nodes to report them out...
	 */
	if (dbg.d_bydriver)
		walk_driver(root_node);
	else
		(void) di_walk_node(root_node, DI_WALK_CLDFIRST, NULL,
		    dump_devs);

	di_fini(root_node);
}

/*
 * utility routines
 */

/*
 * walk_driver is a debugging facility.
 */
static void
walk_driver(di_node_t root)
{
	di_node_t node;

	node = di_drv_first_node(dbg.d_drivername, root);

	while (node != DI_NODE_NIL) {
		(void) dump_devs(node, NULL);
		node = di_drv_next_node(node);
	}
}

/*
 * print out information about this node, returns appropriate code.
 */
/*ARGSUSED1*/
static int
dump_devs(di_node_t node, void *arg)
{
	int ilev = 0;		/* indentation level */
	char *driver_name;

	if (dbg.d_debug) {
		char *path = di_devfs_path(node);
		dprintf("Dump node %s\n", path);
		di_devfs_path_free(path);
	}

	if (dbg.d_bydriver) {
		ilev = 1;
	} else {
		/* figure out indentation level */
		di_node_t tmp = node;
		while ((tmp = di_parent_node(tmp)) != DI_NODE_NIL)
			ilev++;
	}

	indent_to_level(ilev);

	(void) printf("%s", di_node_name(node));

	/*
	 * if this node does not have an instance number or is the
	 * root node (1229946), we don't print an instance number
	 *
	 * NOTE ilev = 0 for root node
	 */
	if ((di_instance(node) >= 0) && ilev)
		(void) printf(", instance #%d", di_instance(node));

	if (opts.o_drv_name) {
		/*
		 * XXX Don't print driver name for root because old prtconf
		 *	can't figure it out.
		 */
		driver_name = di_driver_name(node);
		if (ilev && (driver_name != NULL))
			(void) printf(" (driver name: %s)", driver_name);
	} else if (di_state(node) & DI_DRIVER_DETACHED)
		(void) printf(" (driver not attached)");

	(void) printf("\n");

	if (opts.o_verbose)  {
		if (dump_prop_list("System", ilev+1, node, di_prop_sys_next)) {
			(void) dump_prop_list(NULL, ilev+1, node,
			    di_prop_global_next);
		} else {
			(void) dump_prop_list("System software", ilev+1, node,
				di_prop_global_next);
		}
		(void) dump_prop_list("Driver", ilev+1, node, di_prop_drv_next);
		(void) dump_prop_list("Hardware", ilev+1, node,
				di_prop_hw_next);
		dump_priv_data(ilev+1, node);
	}

	if (!opts.o_pseudodevs && (strcmp(di_node_name(node), "pseudo") == 0))
		return (DI_WALK_PRUNECHILD);
	else
		return (DI_WALK_CONTINUE);
}

/*
 * Returns 0 if nothing is printed, 1 otherwise
 */
static int
dump_prop_list(char *name, int ilev, di_node_t node, di_prop_t (*nxtprop)())
{
	int prop_len, i;
	uchar_t *prop_data;
	char *p;
	di_prop_t prop, next;

	if ((next = nxtprop(node, DI_PROP_NIL)) == DI_PROP_NIL)
		return (0);

	if (name != NULL)  {
		indent_to_level(ilev);
		(void) printf("%s properties:\n", name);
	}

	while (next != DI_PROP_NIL) {
		int maybe_str = 1, npossible_strs = 0;
		prop = next;
		next = nxtprop(node, prop);

		/*
		 * get prop length and value:
		 * private interface--always success
		 */
		prop_len = di_prop_rawdata(prop, &prop_data);

		indent_to_level(ilev +1);
		(void) printf("name <%s> length <%d>",
			di_prop_name(prop), prop_len);

		if (di_prop_type(prop) == DDI_PROP_UNDEF_IT) {
			(void) printf(" -- Undefined.\n");
			continue;
		}

		if (prop_len == 0)  {
			(void) printf(" -- <no value>.\n");
			continue;
		}

		(void) putchar('\n');
		indent_to_level(ilev +1);

		if (prop_data[prop_len - 1] != '\0') {
			maybe_str = 0;
		} else {
			/*
			 * Every character must be a string character or a \0,
			 * and there must not be two \0's in a row.
			 */
			for (i = 0; i < prop_len; i++) {
				if (prop_data[i] == '\0') {
					npossible_strs++;
				} else if (!isascii(prop_data[i]) ||
				    iscntrl(prop_data[i])) {
					maybe_str = 0;
					break;
				}

				if ((i > 0) && (prop_data[i] == '\0') &&
				    (prop_data[i - 1] == '\0')) {
					maybe_str = 0;
					break;
				}
			}
		}

		if (maybe_str) {
			(void) printf("    value ");
			p = (char *)prop_data;
			for (i = 0; i < npossible_strs - 1; i++) {
				(void) printf("'%s' + ", p);
				p += strlen(p) + 1;
			}
			(void) printf("'%s'\n", p);
		} else {
			(void) printf("    value <0x");
			for (i = 0; i < prop_len; ++i)  {
				unsigned char byte;

				byte = (unsigned char)prop_data[i];
				(void) printf("%2.2x", byte);
			}
			(void) printf(">.\n");
		}
	}

	return (1);
}


/* _error([no_perror, ] fmt [, arg ...]) */
static int
_error(const char *opt_noperror, ...)
{
	int saved_errno;
	va_list ap;
	int no_perror = 0;
	const char *fmt;

	saved_errno = errno;

	(void) fprintf(stderr, "%s: ", opts.o_progname);

	va_start(ap, opt_noperror);
	if (opt_noperror == NULL) {
		no_perror = 1;
		fmt = va_arg(ap, char *);
	} else
		fmt = opt_noperror;
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (no_perror)
		(void) fprintf(stderr, "\n");
	else {
		(void) fprintf(stderr, ": ");
		errno = saved_errno;
		perror("");
	}

	return (-1);
}


/*
 * The rest of the routines handle printing the raw prom devinfo (-p option).
 *
 * 128 is the size of the largest (currently) property name
 * 16k - MAXNAMESZ - sizeof (int) is the size of the largest
 * (currently) property value that is allowed.
 * the sizeof (uint_t) is from struct openpromio
 */

#define	MAXNAMESZ	128
#define	MAXVALSIZE	(16384 - MAXNAMESZ - sizeof (uint_t))
#define	BUFSIZE		(MAXNAMESZ + MAXVALSIZE + sizeof (uint_t))
typedef union {
	char buf[BUFSIZE];
	struct openpromio opp;
} Oppbuf;

static int prom_fd;

static int
is_openprom(void)
{
	Oppbuf	oppbuf;
	struct openpromio *opp = &(oppbuf.opp);
	unsigned int i;

	opp->oprom_size = MAXVALSIZE;
	if (ioctl(prom_fd, OPROMGETCONS, opp) < 0)
		exit(_error("OPROMGETCONS"));

	i = (unsigned int)((unsigned char)opp->oprom_array[0]);
	return ((i & OPROMCONS_OPENPROM) == OPROMCONS_OPENPROM);
}

int
do_prominfo(void)
{
	if (promopen(O_RDONLY))  {
		exit(_error("openeepr device open failed"));
	}

	if (is_openprom() == 0)  {
		(void) fprintf(stderr, "System architecture does not "
		    "support this option of this command.\n");
		return (1);
	}

	if (next(0) == 0)
		return (1);
	walk(next(0), 0);
	promclose();
	return (0);
}

static void
walk(int id, int level)
{
	int curnode;

	dump_node(id, level);
	if (curnode = child(id))
		walk(curnode, level+1);
	if (curnode = next(id))
		walk(curnode, level);
}

/*
 * Print all properties and values
 */
static void
dump_node(int id, int level)
{
	Oppbuf	oppbuf;
	struct openpromio *opp = &(oppbuf.opp);

	indent_to_level(level);
	(void) printf("Node");
	if (!opts.o_verbose) {
		print_one("name", level);
		(void) putchar('\n');
		return;
	}
	(void) printf(" %#08x\n", id);

	/* get first prop by asking for null string */
	bzero(oppbuf.buf, BUFSIZE);
	for (;;) {
		/*
		 * get next property name
		 */
		opp->oprom_size = MAXNAMESZ;

		if (ioctl(prom_fd, OPROMNXTPROP, opp) < 0)
			exit(_error("OPROMNXTPROP"));

		if (opp->oprom_size == 0) {
			break;
		}
		print_one(opp->oprom_array, level+1);
	}
	(void) putchar('\n');
}

/*
 * certain 'known' property names may contain 'composite' strings.
 * Handle them here, and print them as 'string1' + 'string2' ...
 */
static int
print_composite_string(const char *var, struct openpromio *opp)
{
	char *p, *q;
	char *firstp;

	if ((strcmp(var, "version") != 0) &&
	    (strcmp(var, "compatible") != 0))
		return (0);	/* Not a known composite string */

	/*
	 * Verify that each string in the composite string is non-NULL,
	 * is within the bounds of the property length, and contains
	 * printable characters or white space. Otherwise let the
	 * caller deal with it.
	 */
	for (firstp = p = opp->oprom_array;
	    p < (opp->oprom_array + opp->oprom_size);
	    p += strlen(p) + 1) {
		if (strlen(p) == 0)
			return (0);		/* NULL string */
		for (q = p; *q; q++) {
			if (!(isascii(*q) && (isprint(*q) || isspace(*q))))
				return (0);	/* Not printable or space */
		}
		if (q > (firstp + opp->oprom_size))
			return (0);		/* Out of bounds */
	}

	for (firstp = p = opp->oprom_array;
	    p < (opp->oprom_array + opp->oprom_size);
	    p += strlen(p) + 1) {
		if (p == firstp)
			(void) printf("'%s'", p);
		else
			(void) printf(" + '%s'", p);
	}
	(void) putchar('\n');
	return (1);
}

/*
 * Print one property and its value.
 */
static void
print_one(const char *var, int level)
{
	Oppbuf	oppbuf;
	struct openpromio *opp = &(oppbuf.opp);
	int i;
	int endswap = 0;

	if (opts.o_verbose) {
		indent_to_level(level);
		(void) printf("%s: ", var);
	}

	(void) strcpy(opp->oprom_array, var);
	if (getpropval(opp) || opp->oprom_size == (uint_t)-1) {
		(void) printf("data not available.\n");
		return;
	}

	if (!opts.o_verbose) {
		(void) printf(" '%s'", opp->oprom_array);
		return;
	}

	/*
	 * Handle printing verbosely
	 */
	if (print_composite_string(var, opp)) {
		return;
	}

	if (!unprintable(opp)) {
		(void) printf(" '%s'\n", opp->oprom_array);
		return;
	}

	(void) printf(" ");
#if defined(i386) || defined(__ia64)
	/*
	 * Due to backwards compatibility constraints x86 int
	 * properties are not in big-endian (ieee 1275) byte order.
	 * If we have a property that is a multiple of 4 bytes,
	 * let's assume it is an array of ints and print the bytes
	 * in little endian order to make things look nicer for
	 * the user.
	 */
	endswap = (opp->oprom_size % 4) == 0;
#endif
	for (i = 0; i < opp->oprom_size; i++) {
		int out;
		if (i && (i % 4 == 0))
			(void) putchar('.');
		if (endswap)
			out = opp->oprom_array[i + (3 - 2 * (i % 4))] & 0xff;
		else
			out = opp->oprom_array[i] & 0xff;

		(void) printf("%02x", out);
	}
	(void) putchar('\n');
}

static int
unprintable(struct openpromio *opp)
{
	int i;

	/*
	 * Is this just a zero?
	 */
	if (opp->oprom_size == 0 || opp->oprom_array[0] == '\0')
		return (1);
	/*
	 * If any character is unprintable, or if a null appears
	 * anywhere except at the end of a string, the whole
	 * property is "unprintable".
	 */
	for (i = 0; i < opp->oprom_size; ++i) {
		if (opp->oprom_array[i] == '\0')
			return (i != (opp->oprom_size - 1));
		if (!isascii(opp->oprom_array[i]) ||
		    iscntrl(opp->oprom_array[i]))
			return (1);
	}
	return (0);
}

static int
promopen(int oflag)
{
	for (;;)  {
		if ((prom_fd = open(opts.o_promdev, oflag)) < 0)  {
			if (errno == EAGAIN)   {
				(void) sleep(5);
				continue;
			}
			if (errno == ENXIO)
				return (-1);
			exit(_error("cannot open %s", opts.o_promdev));
		} else
			return (0);
	}
}

static void
promclose(void)
{
	if (close(prom_fd) < 0)
		exit(_error("close error on %s", opts.o_promdev));
}

static int
getpropval(struct openpromio *opp)
{
	opp->oprom_size = MAXVALSIZE;

	if (ioctl(prom_fd, OPROMGETPROP, opp) < 0)
		return (_error("OPROMGETPROP"));
	return (0);
}

static int
next(int id)
{
	Oppbuf	oppbuf;
	struct openpromio *opp = &(oppbuf.opp);

	bzero(oppbuf.buf, BUFSIZE);
	opp->oprom_size = MAXVALSIZE;
	opp->oprom_node = id;
	if (ioctl(prom_fd, OPROMNEXT, opp) < 0)
		return (_error("OPROMNEXT"));
	return (opp->oprom_node);
}

static int
child(int id)
{
	Oppbuf	oppbuf;
	struct openpromio *opp = &(oppbuf.opp);

	bzero(oppbuf.buf, BUFSIZE);
	opp->oprom_size = MAXVALSIZE;
	opp->oprom_node = id;
	if (ioctl(prom_fd, OPROMCHILD, opp) < 0)
		return (_error("OPROMCHILD"));
	return (opp->oprom_node);
}

/*
 * Get and print the name of the frame buffer device.
 */
int
do_fbname(void)
{
	int	retval;
	char fbuf_path[MAXPATHLEN];

	retval =  modctl(MODGETFBNAME, (caddr_t)fbuf_path);

	if (retval == 0) {
		(void) printf("%s\n", fbuf_path);
	} else {
		if (retval == EFAULT) {
			(void) fprintf(stderr,
			"Error copying fb path to userland\n");
		} else {
			(void) fprintf(stderr,
			"Console output device is not a frame buffer\n");
		}
		return (1);
	}
	return (0);
}

/*
 * Get and print the PROM version.
 */
int
do_promversion(void)
{
	Oppbuf	oppbuf;
	struct openpromio *opp = &(oppbuf.opp);

	if (promopen(O_RDONLY))  {
		(void) fprintf(stderr, "Cannot open openprom device\n");
		return (1);
	}

	opp->oprom_size = MAXVALSIZE;
	if (ioctl(prom_fd, OPROMGETVERSION, opp) < 0)
		exit(_error("OPROMGETVERSION"));

	(void) printf("%s\n", opp->oprom_array);
	promclose();
	return (0);
}

int
do_prom_version64(void)
{
#ifdef	sparc
	Oppbuf	oppbuf;
	struct openpromio *opp = &(oppbuf.opp);
	/*LINTED*/
	struct openprom_opr64 *opr = (struct openprom_opr64 *)opp->oprom_array;

	static const char msg[] =
		"NOTICE: The firmware on this system does not support the "
		"64-bit OS.\n"
		"\tPlease upgrade to at least the following version:\n"
		"\t\t%s\n\n";

	if (promopen(O_RDONLY))  {
		(void) fprintf(stderr, "Cannot open openprom device\n");
		return (-1);
	}

	opp->oprom_size = MAXVALSIZE;
	if (ioctl(prom_fd, OPROMREADY64, opp) < 0)
		exit(_error("OPROMREADY64"));

	if (opr->return_code == 0)
		return (0);

	(void) printf(msg, opr->message);

	promclose();
	return (opr->return_code);
#else
	return (0);
#endif
}
