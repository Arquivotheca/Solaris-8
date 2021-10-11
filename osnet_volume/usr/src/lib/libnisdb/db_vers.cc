/*
 *	db_vers.cc
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)db_vers.cc	1.9	94/05/16 SMI"

#include <stdio.h>
#include <string.h>

#include "db_headers.h"
#include "db_vers.h"

const long unsigned MAXLOW = 32768*32768;

/* Constructor that makes copy of 'other'. */
vers::vers(vers* other)
{
	assign(other);
}

void
vers::assign(vers* other)
{
	if (other == NULL) {
		syslog(LOG_ERR, "vers::vers: making copy of null vers?");
		vers_high = vers_low = time_sec = time_usec = 0;
	} else {
		time_sec = other->time_sec;
		time_usec = other->time_usec;
		vers_low = other->vers_low;
		vers_high = other->vers_high;
	}
}

/*
 * Creates new 'vers' with next higher minor version.
 * If minor version exceeds MAXLOW, bump up major version instead.
 * Set timestamp to that of the current time.
 */
vers*
vers::nextminor()
{
	vers * newvers = new vers;

	if (newvers == NULL) {
		FATAL("vers::nextminor: cannot allocation space",
			DB_MEMORY_LIMIT);
	}

	struct timeval mt;
	gettimeofday(&mt, NULL);

	newvers->time_sec = (unsigned int) mt.tv_sec;
	newvers->time_usec = (unsigned int) mt.tv_usec;
	newvers->vers_low = (this->vers_low + 1);
	newvers->vers_high = (this->vers_high);

	if (newvers->vers_low >= MAXLOW){
		newvers->vers_high++;
		newvers->vers_low = 0;
	}
	return (newvers);
}

/*
 * Creates new 'vers' with next higher major version.
 * Set timestamp to that of the current time.
 */
vers*
vers::nextmajor()
{
	vers * newvers = new vers;

	if (newvers == NULL) {
		FATAL("vers::nextminor: cannot allocation space",
			DB_MEMORY_LIMIT);
	}

	struct timeval mt;
	gettimeofday(&mt, NULL);

	newvers->time_sec = (unsigned int) mt.tv_sec;
	newvers->time_usec = (unsigned int) mt.tv_usec;
	newvers->vers_low = 0;
	newvers->vers_high = (this->vers_high+1);

	return (newvers);
}

/*
 * Predicate indicating whether this vers is earlier than 'other' in
 * terms of version numbers.
*/
bool_t
vers::earlier_than(vers *other)
{
	if (other == NULL) {
		syslog(LOG_ERR,
			"vers::earlier_than: comparing against null vers");
		return (FALSE);
	}

	if (other->vers_high > vers_high) return (TRUE);
	else if (other->vers_high < vers_high) return (FALSE);
	else if (other->vers_low > vers_low) return (TRUE);
	else return (FALSE);
}

/* Print the value of this 'vers' to specified file. */
void
vers::print(FILE* file)
{
	char *thetime;
	thetime = ctime((long *) (&(time_sec)));
	thetime[strlen(thetime)-1] = 0;
	fprintf(file, "version=%u.%u %s:%u",
		vers_high,
		vers_low,
		/* time_sec, */
		thetime,
		time_usec);
}
