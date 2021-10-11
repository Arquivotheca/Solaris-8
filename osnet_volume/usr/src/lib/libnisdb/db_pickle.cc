/*
 *	db_pickle.cc
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)db_pickle.cc	1.6	97/04/21 SMI"

/* #include <sys/types.h> */
#include <stdio.h>
/* #include <syslog.h> */
#include <string.h>
#include <unistd.h>
#include "db_headers.h"
#include "db_pickle.h"

/* Constructor.  Creates pickle_file with given name and mode. */
pickle_file::pickle_file(char* f, pickle_mode m)
{
	if ((filename = strdup(f)) == NULL)
		FATAL("pickle_file::pickle_file: cannot allocate space",
			DB_MEMORY_LIMIT);

	mode = m;
}

/*
 * Opens pickle_file with mode specified with constructor.
 * Returns TRUE if open was successful; FALSE otherwise.
 */
bool_t
pickle_file::open()
{
	if (mode == PICKLE_READ) {
		file = fopen(filename, "r");
		if (file)
			xdrstdio_create(&(xdr), file, XDR_DECODE);
	} else if (mode == PICKLE_WRITE) {
		file = fopen(filename, "w");
		if (file) {
			setvbuf(file, NULL, _IOFBF, 81920);
			xdrstdio_create(&(xdr), file, XDR_ENCODE);
		}
	} else if (mode == PICKLE_APPEND) {
		file = fopen(filename, "a");
		if (file)
			xdrstdio_create(&(xdr), file, XDR_ENCODE);
	}
	if (file == NULL) {
		return (FALSE);
	}
	return (TRUE);
}


/* Closes pickle_file.  Returns 0 if successful; -1 otherwise. */
int
pickle_file::close()
{
	xdr_destroy(&(xdr));
	return (fclose(file));
}


/*
 * dump or load data structure to/from 'filename' using function 'f'.
 * dump or load is determined by 'mode' with which pickle_file was created.
 * Returns 0 if successful; 1 if file cannot be opened in mode
 * specified; -1 if transfer failed do to encoding/decoding errors.
*/
int
pickle_file::transfer(pptr p, bool_t (*f) (XDR*, pptr))
{
	if (open()) {
		if ((f)(&xdr, p) == FALSE) {
			close();
			return (-1);
		} else {
			fsync(fileno(file));
			return (close());
		}
	}
	return (1);
}
