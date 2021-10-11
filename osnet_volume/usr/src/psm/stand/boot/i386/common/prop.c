/*
 * Copyright (c) 1992-1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prop.c	1.9	99/06/06 SMI"

/* property processing - getprop/getproplen/setprop commands */

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/bsh.h>
#include <sys/salib.h>
#include "devtree.h"

#define	__ctype _ctype		/* Incredably stupid hack used by	*/
#include <ctype.h>		/* ".../stand/lib/i386/subr_i386.c"	*/

extern struct bootops *bop;
extern struct dnode *active_node;

extern int boldgetproplen(struct bootops *bop, char *name);
extern int boldgetprop(struct bootops *bop, char *name, void *value);
extern int boldsetprop(struct bootops *bop, char *name, char *value);
extern char *boldnextprop(struct bootops *bop, char *prevprop);
extern int bgetproplen(struct bootops *, char *, phandle_t);
extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int bnextprop(struct bootops *, char *, char *, phandle_t);
extern void putchar();
extern unsigned long strtoul();
extern unsigned char *var_ops(unsigned char *name, unsigned char *value,
    int op);

static int
is_string_prop(char *buf, int len)
{
	/*
	 *  Check for printable chars:
	 *
	 *    This routine verifies that the data contained in the "len"-byte
	 *    "buf"fer is a printable ASCII string.  It returns a non-zero
	 *    value if this is the case.
	 */

	if (buf[--len] == '\0') {
		/*
		 *  Well, the string is null-terminated.  Now let's see if it
		 *  contains any non-printable characters.
		 */
		while (len-- > 0) {

			if (!isascii(*buf) || !isprint(*buf)) {
				/*
				 *  This character cannot be printed, hence
				 *  data in the buffer can't be a string!
				 */
				return (0);
			}
			buf += 1;
		}
		return (1);
	}
	return (0);
}

static void
print_prop(char *buf, int len)
{
	/*
	 *  Print a property value:
	 *
	 *    This routine is used by the "getprop" and ".properties" command
	 *    to print a property value on the standard output.
	 */
	int x = is_string_prop(buf, len);
	int lx = len - x;
	char *qt;

	printf("%s", qt = (x && ((lx < 0) || strchr(buf, ' '))) ? "\"" : "");

	for (; lx-- > 0; buf++) {
		/*
		 *  The manner in which the value is printed depends on
		 *  whether or not it's a valid character string ...
		 */
		if (x > 0) {
			/*
			 *  .. String properties are printed more-or-less as
			 *  is, except that we do watch for certain control
			 *  chars and display their escaped variants.
			 */
			switch (*buf) {
			case '\n': printf("\\n");  break;
			case '\t': printf("\\t");  break;
			case '\r': printf("\\r");  break;
			case '\b': printf("\\b");  break;
			case '"' : printf("\\\""); break;

			default: putchar(*buf);   break;
			}

		} else {
			/*
			 *  .. Binary values are displayed as a sequence of hex
			 *  backslach escapes.
			 */
			static char hextab[] = "0123456789ABCDEF";

			printf("\\x");
			putchar(hextab[(*buf >> 4) & 15]);
			putchar(hextab[*buf & 15]);
		}
	}
	printf("%s", qt);
}

int
get_bin_prop(char *cp, char **bufp, char *cmd)
{
	/*
	 *  Parse a binary property array:
	 *
	 *  A couple of commands will take a comma-separated list of integer
	 *  values which ultimately become a property value.  This routine
	 *  parses the integer list at "*cp" and places the corresponding
	 *  array of integers in a dynamically allocated buffer whose
	 *  address is returned at "*bufp".  It returns the size of the
	 *  integer array, or -1 if there's an error (after printing an
	 *  appropriate error msg).
	 */
	unsigned *up;
	char *buf;
	int n = 1;

	for (buf = strchr(cp, ','); buf; buf = strchr(buf+1, ',')) n++;
	buf = bkmem_alloc(n *= sizeof (unsigned));
	*bufp = buf;

	if ((up = (unsigned *)buf) != 0) {
		/*
		 *  We now have a temporary work area at "up" into which we can
		 *  convert the input text.  Step thru the value argument using
		 *  "strtol" to convert the next digit and advance the input
		 *  pointer.
		 */
		while (*cp != '\0') {
			/*
			 *  Loop terminates when we get to the end of the
			 *  argument string, or when we encounter an
			 *  unexpected separator.
			 *  If the "cp" register points to a null when the loop
			 *  exits, we have a valid property value.
			 */
			*up++ = strtoul(cp, &cp, 0);

			if (*cp && (*cp++ != ',')) {
				/*
				 *  There should be a comma here, but there
				 *  isn't.  This is a syntax error!
				 */
				cp--;
				printf("%s: syntax error. ", cmd);
				printf("Expected comma, got \'%c\' (0x%x).\n",
				    *cp, *cp);
				return (-1);
			}
		}

		return (n);

	} else {
		/*
		 *  Can't get memory for the property buffer!
		 */
		printf("%s: no memory\n", cmd);
	}
	return (-1);
}

void
getprop_cmd(int argc, char **argv)
{
	/*
	 *  Assign property value to a variable:
	 *
	 *    This command reads the property named by its first argument into
	 *    the boot variable named by its second argument (or prints the re-
	 *    sult on stdout if the second argument is omitted).  Only property
	 *    values that are null-terminated strings can be assigned to boot
	 *    variables!
	 */
	if (argc >= 2) {
		/*
		 *  Command is properly formatted.  Verify that the property
		 *  exists and that its value is a printable string.  The
		 *  "bgetproplen" routine generates an error message if the
		 *  named property does not exist.
		 */
		int length = bgetproplen(bop, argv[1], active_node->dn_nodeid);

		if (length >= 0) {
			/*
			 *  Property exists, obtain its value.  We dynamically
			 *  allocate a buffer to hold this value so that we
			 *  don't have to impose artificial length
			 *  restrictions.
			 *
			 *  NOTE: Zero-length properties are indicated by a
			 *  null string.
			 */
			char *buf = (length ? bkmem_alloc(length) : "");

			if (buf != (char *)0) {
				/*
				 *  We've got our buffer, now read in the
				 *  property value and make sure it's
				 *  printable.  The "bgetprop" routine should
				 *  not fail -- we've already verified
				 *  everything with "bgetproplen".
				 */
				(void) bgetprop(bop, argv[1], buf, length,
				    active_node->dn_nodeid);

				if (argc < 3) {
					/*
					 *  Caller omitted the second argument,
					 *  which means we're supposed to
					 *  display the property on stdout.
					 */
					print_prop(buf, length);
					putchar('\n');

				} else if (is_string_prop(buf, length)) {
					/*
					 *  Property is a printable string, go
					 *  ahead and assign it to the named
					 *  boot variable.
					 */
					(void) var_ops((unsigned char *)argv[2],
					    (unsigned char *)buf, SET_VAR);

				} else {
					/*
					 *  Property has a binary encoding.
					 *  Print an error message.
					 */
					printf("getprop: value is not a "
					    "string\n");
				}

				if (length) bkmem_free(buf, length);

			} else {
				/*
				 *  Couldn't buy a buffer to hold the
				 *  property value.  We must be running very
				 *  low on memory!
				 */
				printf("getprop: no memory\n");
				return;
			}
		} else {
			/*
			 *  Specified property does not exist!
			 */
			printf("getprop: %s not found\n", argv[1]);
		}
	} else {
		/*
		 *  Ill-formed command syntax.  Generate usage message and bail
		 *  out.
		 */
		printf("usage: getprop prop-name [var-name]\n");
	}
}

void
getproplen_cmd(int argc, char **argv)
{
	/*
	 *  Assign property length to a boot variable:
	 *
	 *    This routine is similar to "getprop_cmd", except that it assigns
	 *    the length of the property named by its first argument to the
	 *    boot variable named by the second argument.
	 */
	if (argc >= 2) {
		/*
		 *  Command is well formed, find the length of the named
		 *  property and convert it to hex for display purposes.
		 */
		int length = bgetproplen(bop, argv[1], active_node->dn_nodeid);

		if (length >= 0) {
			/*
			 *  The property exists.  Convert the binary length to
			 *  a printable string and assign the result to the
			 *  named variable (or print it if caller didn't give
			 *  us a variable).
			 */
			char buf[16];
			(void) sprintf(buf, "%d", length);

			if (argc > 2) {
				/* Caller wants length assigned to variable */
				(void) var_ops((unsigned char *)argv[2],
				    (unsigned char *)buf, SET_VAR);
			} else {
				/* Caller wants length printed */
				printf("%s\n", buf);
			}
		} else {
			/*
			 *  The specified property does not exist!
			 */
			printf("getproplen: %s not found\n", argv[1]);
		}
	} else {
		/*
		 *  Syntax error.  Generate usage message and bail out!
		 */
		printf("usage: getproplen prop-name [var-name]\n");
	}
}

void
setprop_cmd(int argc, char **argv, int *lenv)
{
	/*
	 *  Set a property value:
	 *
	 *    Sets the value of the property named by the first argument to the
	 *    value of the second argument.  Binary properties can be set by
	 *    judicious use of backslashes.
	 */
	if (argc >= 2) {
		/*
		 *  Syntax is well-formed, now assign the property value.
		 *  The only tricky part here is getting the length right (due
		 *  to the possible embedded nulls).  Fortunately, the lenv[]
		 *  array contains the total number of chars extracted by the
		 *  parser, so we'll just use that.
		 */
		(void) bsetprop(bop, argv[1], argv[2],
		    (argc > 2) ? lenv[2] : 0, active_node->dn_nodeid);
	} else {
		/*
		 *  Syntax error; print usage message and bail out!
		 */
		printf("setprop: prop-name prop-value\n");
	}
}

void
setbin_cmd(int argc, char **argv)
{
	/*
	 *  Set a binary property value:
	 *
	 *    This is similar to "setprop" except that the value string (second
	 *    argument) is assumed to be a list of integer values separated by
	 *    commas.  The integers can be expressed in decimal, octal, or hex
	 *    using "strtol" notation.
	 */
	if (argc == 3) {

		/*
		 *  Syntax is valid, calculate the number of integers that go
		 *  into the value array by counting the number of commas in
		 *  the value string.  We can then buy an input buffer of
		 *  appropriate size.
		 */
		int n;
		char *buf;

		if ((n = get_bin_prop(argv[2], &buf, "setbinprop")) > 0) {
			/*
			 *  We've parsed the input text, now bind it to the
			 *  named property and free up the buffer.
			 */
			(void) bsetprop(bop, argv[1], buf, n,
			    active_node->dn_nodeid);
			bkmem_free(buf, n);
		}
	} else {
		/*
		 *  Syntax error; print usage message and bail out!
		 */
		printf("setprop: prop-name prop-value\n");
	}
}

/*ARGSUSED*/
void
props_cmd(int argc, char **argv)
{
	/*
	 *  Display properties:
	 *
	 *    Walks the active node's property list printing the name and value
	 *    of each property it encounters.
	 */
	char *buf = 0;
	char  name_buf[MAX1275NAME];

	while (bnextprop(bop, buf, name_buf, active_node->dn_nodeid) > 0) {
		/*
		 *  The name of the next property has been placed in the
		 *  "name_buf".  Extract the property itself and then print it.
		 */
		int len = bgetproplen(bop, name_buf, active_node->dn_nodeid);
		printf("%s=", name_buf);

		if ((len > 0) && (buf = bkmem_alloc(len))) {
			/*
			 *  The "len" register gives the full length
			 * of the next property, and "buf"
			 * points to a dynamically allocated
			 * buffer into which we can "bgetprop" the
			 * appropriate value.
			 */
			(void) bgetprop(bop, name_buf, buf, len,
			    active_node->dn_nodeid);
			print_prop(buf, len);
			bkmem_free(buf, len);
		}

		putchar('\n');
		buf = name_buf;
	}
}
