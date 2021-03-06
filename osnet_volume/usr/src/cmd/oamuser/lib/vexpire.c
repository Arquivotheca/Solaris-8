/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)vexpire.c	1.4	97/01/23 SMI"       /* SVr4.0 1.5 */

#include	<sys/types.h>
#include	<time.h>
#include	<users.h>

extern long p_getdate();

/*
	Validate an expiration date string
*/
int
valid_expire( string, expire )
char *string;
time_t *expire;
{
	time_t tmp, now;
	struct tm *tm;

	if( !(tmp = (time_t) p_getdate( string ) ) )
		return( INVALID );

	now = time( (time_t *)0 );
	
	/* Make a time_t for midnight tonight */
	tm = localtime( &now );
	now -= tm->tm_hour * 60*60 + tm->tm_min * 60 + tm->tm_sec;
	now += 24 * 60 * 60;

	if( tmp < now ) return( INVALID );

	if( expire ) *expire = now;

	return( UNIQUE );
}
