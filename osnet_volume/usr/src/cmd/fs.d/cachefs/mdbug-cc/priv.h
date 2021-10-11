// ------------------------------------------------------------
//
//			priv.h
//
//    Internal header file for the mdbug package.
//

#pragma ident   "@(#)priv.h 1.1     94/10/28 SMI"
// Copyright (c) 1994 by Sun Microsystems, Inc.

// .LIBRARY base
// .NAME dbug_state - used by dbug_routine to maintain state

// .SECTION Description
// The dbug_state class is used by the dbug_routine class to maintain
// state established by the dbug_push() macro.
// The priv.h include file is also used to store constructs used internally
// by the mdbug package.

#ifndef PRIV_H
#define	PRIV_H

// DBUG_DOS or DBUG_UNIX should be defined in the makefile to 1

// Define various shorthand notations.
#define	boolean int
#define	TRUE 1
#define	FALSE 0
#define	NOT !
#define	XOR ^
typedef unsigned long u_long;
typedef unsigned int u_int;
typedef unsigned short u_short;
typedef unsigned char u_char;
#define	MAX(x, y) (((x) > (y)) ? (x) : (y))
#define	MIN(x, y) (((x) > (y)) ? (y) : (x))

// Determine which way the stack grows
#if DBUG_DOS || DBUG_UNIX
const int GROWDOWN = TRUE;
#else
const int GROWDOWN = FALSE;
#endif

// Manifest constants which may be "tuned" if desired.
const int PRINTBUF =	1024;	/* Print buffer size */
const int INDENT =	4;	/* Indentation per trace level */
const int MAXDEPTH =	200;	/* Maximum trace depth default */

// macros for determining access to a file
inline boolean
file_exists(const char *pathname)
{
	return (access(pathname, F_OK) == 0);
}

inline boolean
file_writable(const char *pathname)
{
	return (access(pathname, W_OK) == 0);
}


// This class is used to maintain the state established by the
// push call.
class dbug_state
{
private:
public:
	dbug_state(int level);
	~dbug_state();

	boolean	 sf_trace:1;	// TRUE if tracing is on
	boolean	 sf_debug:1;	// TRUE if debugging is on
	boolean	 sf_file:1;	// TRUE if file name print enabled
	boolean	 sf_line:1;	// TRUE if line number print enabled
	boolean	 sf_depth:1;	// TRUE if function nest level print enabled
	boolean	 sf_process:1;	// TRUE if process name print enabled
	boolean	 sf_number:1;	// TRUE if number each line
	boolean	 sf_pid:1;	// TRUE if identify each line with pid
	boolean	 sf_stack:1;	// TRUE if should print stack depth
	boolean	 sf_time:1;	// TRUE if should print time information
	boolean	 sf_didopen:1;	// TRUE if opened the log file
	boolean	 sf_thread:1;	// TRUE if should print thread information
	int	 s_maxdepth;	// Current maximum trace depth
	int	 s_delay;	// Delay amount after each output line
	u_int	 s_level;	// Current function nesting level
	time_t	 s_starttime;	// Time push was done
	FILE	*s_out_file;	// Current output stream
	flist	 s_functions;	// List of functions
	flist	 s_pfunctions;	// List of profiled functions
	flist	 s_keywords;	// List of debug keywords
	flist	 s_processes;	// List of process names

	dbug_state	*s_next;	// pointer to next pushed state
};

#endif /* PRIV_H */
