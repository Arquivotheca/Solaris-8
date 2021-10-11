/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_X500TRACE_HH
#define	_X500TRACE_HH

#pragma ident	"@(#)X500Trace.hh	1.1	96/03/31 SMI"


/*
 * X.500 trace messages
 */


class X500Trace
{

public:

#ifdef DEBUG
	void			x500_trace(char *format, ...) const;
#else
	void			x500_trace(char *, ...) const;
#endif


	X500Trace() {};
	virtual ~X500Trace() {};
};


#endif	/* _X500TRACE_HH */
