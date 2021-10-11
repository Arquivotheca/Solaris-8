/*
 *	db_pickle.h
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)db_pickle.h	1.4	92/07/14 SMI"

#ifndef PICKLE_H
#define	PICKLE_H

/*
	'pickle' is the package for storing data structures into files.
	'pickle_file' is the base class.  Classes that inherit this base
	class need to instantiate the virtual function 'dump'.
*/

enum pickle_mode {
	PICKLE_READ, PICKLE_WRITE, PICKLE_APPEND
};

typedef enum pickle_mode pickle_mode;

typedef void* pptr;		/* pickle pointer */

class pickle_file {
    protected:
	FILE *file;		/* file handle */
	pickle_mode mode;
	XDR xdr;
	char* filename;
    public:

	/* Constructor.  Creates pickle_file with given name and mode. */
	pickle_file(char*, pickle_mode);

	~pickle_file()  { delete filename; }

	/*
	 * Opens pickle_file with mode specified with constructor.
	 * Returns TRUE if open was successful; FALSE otherwise.
	 */
	bool_t open();

	/* Closes pickle_file.  Returns 0 if successful; -1 otherwise. */
	int close();

	/*
	dump or load data structure to/from 'filename' using function 'f'.
	dump/load is determined by 'mode' with which pickle_file was created.
	Returns 0 if successful; 1 if file cannot be opened in mode
	specified; -1 if transfer failed do to encoding/decoding errors.
	*/
	int transfer(pptr, bool_t (*f) (XDR*, pptr));
};
#endif PICKLE_H
