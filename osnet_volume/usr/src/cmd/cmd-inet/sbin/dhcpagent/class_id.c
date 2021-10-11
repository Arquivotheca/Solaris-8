/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)class_id.c	1.1	99/04/09 SMI"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/openpromio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>			/* sprintf() */
#include <unistd.h>

/*
 * opp_zalloc(): allocates and initializes a struct openpromio
 *
 *   input: size_t: the size of the variable-length part of the openpromio
 *          const char *: an initial value for oprom_array, if non-NULL
 *  output: struct openpromio: the allocated, initialized openpromio
 */

static struct openpromio *
opp_zalloc(size_t size, const char *prop)
{
	struct openpromio *opp = malloc(sizeof (struct openpromio) + size);

	if (opp != NULL) {
		(void) memset(opp, 0, sizeof (struct openpromio) + size);
		opp->oprom_size = size;
		if (prop != NULL)
			(void) strcpy(opp->oprom_array, prop);
	}
	return (opp);
}

/*
 * goto_rootnode(): moves to the root of the devinfo tree
 *
 *   input: int: an open descriptor to /dev/openprom
 *  output: int: nonzero on success
 */

static int
goto_rootnode(int prom_fd)
{
	struct openpromio op = { sizeof (int), 0 };

	/* zero it explicitly since a union is involved */
	op.oprom_node = 0;
	return (ioctl(prom_fd, OPROMNEXT, &op) == 0);
}

/*
 * return_property(): returns the value of a given property
 *
 *   input: int: an open descriptor to /dev/openprom
 *          const char *: the property to look for in the current devinfo node
 *  output: the value of that property (dynamically allocated)
 */

static char *
return_property(int prom_fd, const char *prop)
{
	int 			proplen;
	char			*result;
	struct openpromio	*opp = opp_zalloc(strlen(prop) + 1, prop);

	if (opp == NULL)
		return (NULL);

	if (ioctl(prom_fd, OPROMGETPROPLEN, opp) == -1) {
		free(opp);
		return (NULL);
	}

	proplen = opp->oprom_len;
	if (proplen > (strlen(prop) + 1)) {
		free(opp);
		opp = opp_zalloc(proplen, prop);
		if (opp == NULL)
			return (NULL);
	}

	if (ioctl(prom_fd, OPROMGETPROP, opp) == -1) {
		free(opp);
		return (NULL);
	}

	result = strdup(opp->oprom_array);
	free(opp);
	return (result);
}

/*
 * sanitize_class_id(): translates the class id into a canonical format,
 *			so that it can be used easily with dhcptab(4).
 *
 *   input: char *: the class id to canonicalize
 *  output: void
 */

static void
sanitize_class_id(char *src_ptr)
{
	char	*dst_ptr = src_ptr;

	/* remove all spaces and change all commas to periods */
	while (*src_ptr != '\0') {

		switch (*src_ptr) {

		case ' ':
			break;

		case ',':
			*dst_ptr++ = '.';
			break;

		default:
			*dst_ptr++ = *src_ptr;
			break;
		}
		src_ptr++;
	}
	*dst_ptr = '\0';
}

/*
 * get_class_id(): retrieves the class id from the prom, then canonicalizes it
 *
 *   input: void
 *  output: char *: the class id (dynamically allocated and sanitized)
 */

char *
get_class_id(void)
{
	int	prom_fd;
	char    *name, *class_id = NULL;

	prom_fd = open("/dev/openprom", O_RDONLY);
	if (prom_fd == -1)
		return (NULL);

	if (goto_rootnode(prom_fd) != 0) {

		/*
		 * the `name' property has a value similar to the
		 * result of `uname -i', modulo some stylistic issues
		 * we fix up in sanitize_class_id().
		 */

		name = return_property(prom_fd, "name");
		if (name == NULL) {
			(void) close(prom_fd);
			return (NULL);
		}

		sanitize_class_id(name);

		/*
		 * make sure the class id always starts with "SUNW.";
		 * since the "SUNW." in the name property is only
		 * present for sun hardware, intel boxes do not have
		 * this prepended yet.
		 */

		if (strncmp("SUNW.", name, strlen("SUNW.")) != 0) {

			class_id = malloc(strlen(name) + strlen("SUNW.") + 1);
			if (class_id != NULL)
				(void) sprintf(class_id, "SUNW.%s", name);
			free(name);
		} else
			class_id = name;
	}

	(void) close(prom_fd);
	return (class_id);
}
