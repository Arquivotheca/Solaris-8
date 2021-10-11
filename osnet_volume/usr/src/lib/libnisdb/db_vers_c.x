/*
 *	db_vers_c.x
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

%#pragma ident	"@(#)db_vers_c.x	1.3	92/07/14 SMI"

#if RPC_HDR
%#ifndef _DB_VERS_H
%#define _DB_VERS_H
#endif RPC_HDR

%/* 'vers' is the version identifier.  */

#if RPC_HDR || RPC_XDR
#ifdef USINGC
struct vers {
	u_int vers_high;
	u_int vers_low;
	u_int time_sec;
	u_int time_usec;
};
#endif USINGC
#endif RPC_HDR

#ifndef USINGC
#ifdef RPC_HDR
%class vers {
%  unsigned int vers_high;     /* major version number, tracks checkpoints */
%  unsigned int vers_low;      /* minor version number, tracks updates. */
%  unsigned int time_sec;      /* time stamp */
%  unsigned int time_usec;       
% public:
%/* No argument constructor.  All entries initialized to zero. */
%  vers() { vers_high = vers_low = time_sec = time_usec = 0; }
%
%/* Constructor that makes copy of 'other'. */
%  vers( vers *other );
%
%/* Constructor:  create version with specified version numbers */
%  vers( unsigned int high, unsigned int low) 
%     { vers_high = high; vers_low = low; time_sec = time_usec = 0; } 
%
%/* Creates new 'vers' with next higher minor version.
%   If minor version exceeds MAXLOW, bump up major version instead.
%   Set timestamp to that of the current time. */
%  vers* nextminor();
%
%/* Creates new 'vers' with next higher major version.
%   Set timestamp to that of the current time. */
%  vers* nextmajor();
%
%/* Set this 'vers' to hold values found in 'others'. */
%  void assign( vers *other );
%
%/* Predicate indicating whether this vers is earlier than 'other' in
%   terms of version numbers. */
%  bool_t earlier_than( vers *other );
%
%/* Print the value of this 'vers' to specified file. */
%  void print( FILE *file );
%
%/* Zero out this vers. */
%  void zero() { vers_high = vers_low = time_sec = time_usec = 0; } 
%
%/* Predicate indicating whether this vers is equal to 'other'. */
%  bool_t  equal( vers *other) 
%  { return (other != NULL &&
%	     vers_high == other->vers_high &&
%	    vers_low == other->vers_low &&
%	    time_sec == other->time_sec &&
%	    time_usec == other->time_usec) ; } 
%};
%
#endif RPC_HDR
#endif USINGC

#if RPC_HDR
%#endif VERS_H
#endif RPC_HDR
