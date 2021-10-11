// ------------------------------------------------------------
//
//			mdbug.h
//
//	Include file for the mdbug class.
//

#pragma ident   "@(#)mdbug.h 1.1     94/10/28 SMI"
// Copyright (c) 1994 by Sun Microsystems, Inc.

// .LIBRARY base
// .NAME mdbug - macros for debugging C++ programs.
// .FILE dbug.cc
// .FILE mdbug.h

// .SECTION Description
// The mdbug package provides a set of macros for debugging C++ programs.
// Features include tracing function entry and exit points, printing
// of debug messages, and heap corruption detection.  All features can
// be selectively enabled at run time using command line options.  Also
// defining the macro DBUG_OFF removes all mdbug code from the compilation.

#ifndef MDBUG_H
#define	MDBUG_H

#define	DBUG_STMT(A) do { A } while (0)

#ifndef DBUG_OFF

class dbug_routine {
private:
	static int		 sd_on;		// Debug on/off flag.
	static long		 sd_lineno;	// Output line number.
	static const char	*sd_process;	// Current process.
	static class dbug_state	*sd_push;	// Push information.
	static void dbug_thread_exit(void *data);

	const char		*d_func;	// Name of current function.
	const char		*d_file;	// Name of current file.
	dbug_routine		*d_prev;	// Callers dbug_routine object.
	int			 d_leaveline;	// Exit line from routine.

public:
	dbug_routine(int line, const char *file, const char *function);
	~dbug_routine();
	void		 db_leave(int line);
	int		 db_keyword(const char *keyword);
	void		 db_pargs(int line, const char *keyword);
	void		 db_printf(const char *, ...);
	void		 db_traceprint(int line, const char *keyword);
	void		 db_assert(int line, const char *msgp);
	void		 db_precond(int line, const char *msgp);
	static const char *db_push(const char *);
	static void	 db_pop();
	void		 db_process(const char *);
	static int	 db_debugon();
};

#define	dbug_enter(A)		dbug_routine _did(__LINE__, __FILE__, A)
#define	dbug_leave()		_did.db_leave(__LINE__)
#define	dbug_traceprint(KEY)	_did.db_traceprint(__LINE__, KEY)
#define	dbug_push(A)		dbug_routine::db_push(A)
#define	dbug_pop()		dbug_routine::db_pop()
#define	dbug_process(A)		_did.db_process(A)

#define	dbug_assert(A) \
	    DBUG_STMT(if (!(A)) { _did.db_assert(__LINE__, #A); })
#define	dbug_precond(A) \
	    DBUG_STMT(if (!(A)) { _did.db_precond(__LINE__, #A); })

#define	dbug_execute(KEY, CODE) \
    DBUG_STMT(if (dbug_routine::db_debugon()) \
	    { if (_did.db_keyword(KEY)) { CODE } })

#define	dbug_print(KEY, ARGS) \
    DBUG_STMT(if (dbug_routine::db_debugon()) \
	    { _did.db_pargs(__LINE__, KEY); _did.db_printf ARGS; })

// ------------------------------------------------------------
//
//		db_debugon
//
// Description:
//   Returns 1 if debugging is currently enabled, 0 otherwise.
// Arguments:
// Returns:
// Errors:
// Preconditions:

inline int
dbug_routine::db_debugon()
{
	return (sd_on);
}

#else /* if DBUG_OFF */

#define	dbug_enter(A)			0
#define	dbug_leave()			0
#define	dbug_traceprint(KEY)		0
#define	dbug_push(A)			0
#define	dbug_pop()			0
#define	dbug_process(A)			0
#define	dbug_execute(KEY, CODE)		0
#define	dbug_print(KEY, ARGS)		0
#define	dbug_assert(A)			0
#define	dbug_precond(A)			0

#endif /* DBUG_OFF */
#endif /* MDBUG_H */
