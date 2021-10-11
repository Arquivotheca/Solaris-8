#pragma ident	"@(#)sbfocus_enter.c	1.5	96/05/08 SMI"

/*
 *	Copyright (c) 1989,1990 Sun Microsystems, Inc.  All Rights Reserved.
 *	Sun considers its source code as an unpublished, proprietary
 *	trade secret, and it is available only under strict license
 *	provisions.  This copyright notice is placed here only to protect
 *	Sun in the event the source is deemed a published work.  Dissassembly,
 *	decompilation, or other means of reducing the object code to human
 *	readable form is prohibited by the license agreement under which
 *	this code is provided to the user or company in possession of this
 *	copy.
 *	RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the
 *	Government is subject to restrictions as set forth in subparagraph
 *	(c)(1)(ii) of the Rights in Technical Data and Computer Software
 *	clause at DFARS 52.227-7013 and in similar clauses in the FAR and
 *	NASA FAR Supplement.
 */

#include "sbfocus_enter.h"

extern  void   sbfocus_symbol();
extern  void   sbfocus_close();

/*
 * sbfocus_symbol() will write one symbol to a pipe that has the program
 * "sbfocus" at the receiving end. If the program has not been started yet,
 * it is started, and the pipe established. "sbfocus" is started with the
 * function arguments "type" and "name" as its arguments, in that order.
 *
 * sbfocus_symbol() should be called with four arguments:
 *	data	Pointer to a Sbld struct that the caller has allocated in
 *		permanent storage. It must be the same struct for all related
 *		calls to sbfocus_symbol().
 *	name	This is the string name of the library/executable being built.
 *	type	A string, should be one of:
 *                      "-a": Building a archived library
 *			"-s": Building a shared library
 *			"-x": Building an executable
 *			"-r": Concatenating object files
 *	symbol	The string that should be written to "sbfocus". If this
 *		argument is NULL "sbfocus" is started, but no symbol is
 *		written to it.
 */

void
sbfocus_symbol(data, name, type, symbol)
	Sbld	data;
	char	*name;
	char	*type;
	char	*symbol;
{
	int	fd[2];

	if (data->failed) {
		return;
	}

	if (data->fd == NULL) {
		data->failed = 0;
		(void) pipe(fd);

		switch (vfork()) {
		case -1:
			(void) fprintf(stderr,
			"vfork() failed. SourceBrowser data will be lost.\n");
			data->failed = 1;
			(void) close(fd[0]);
			(void) close(fd[1]);
			return;

		/*
		 * Child process
		 */
		case 0:
			(void) dup2(fd[0], fileno(stdin));
			(void) close(fd[1]);
			(void) execlp("sbfocus", "sbfocus", type, name, 0);
			data->failed = 1;
			_exit(1);

		/*
		 * Parent process
		 */
		default:
			if (data->failed) {
				(void) fprintf(stderr,
				"`sbfocus' would not start."
				" SourceBrowser data will be lost.\n");
				return;
			}
			(void) close(fd[0]);
			data->fd = fdopen(fd[1], "w");
			break;
		}
	}
	if (symbol != NULL) {
		(void) fputs(symbol, data->fd);
		(void) putc('\n', data->fd);
	}
}

/*
 * sbfocus_close() will close the pipe to "sbfocus", causing it to terminate.
 *
 * sbfocus_close() should be called with one argument, a pointer to the data
 * block used with sbfocus_symbol().
 */
void
sbfocus_close(data)
	Sbld	data;
{
	if ((data->fd != NULL) && (data->failed == 0)) {
		(void) fclose(data->fd);
	}
	data->fd = NULL;
	data->failed = 0;
}
