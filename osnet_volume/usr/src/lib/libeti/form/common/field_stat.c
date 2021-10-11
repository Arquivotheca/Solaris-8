/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)field_stat.c	1.5	97/09/17 SMI" /* SVr4.0 1.2 */

/*LINTLIBRARY*/

#include <sys/types.h>
#include "utility.h"

int
set_field_status(FIELD *f, int status)
{
	f = Field(f);

	if (status)
		Set(f, USR_CHG);
	else
		Clr(f, USR_CHG);

	return (E_OK);
}

int
field_status(FIELD *f)
{
/*
 * field_status may not be accurate on the current field unless
 * called from within the check validation function or the
 * form/field init/term functions.
 * field_status is always accurate on validated fields.
 */
	if (Status(Field(f), USR_CHG))
		return (TRUE);
	else
		return (FALSE);
}
