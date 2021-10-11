/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_DOSTYPES_WRAPPER_H
#define	_DOSTYPES_WRAPPER_H

#ident "@(#)dostypes.h	1.2	97/04/07 SMI"

/*
 *	The real dostypes.h file has been moved so that the realmode
 *	DDK does not need both inc and common/include header files.
 *	This wrapper eliminates the need to fix all makefiles in the
 *	realmode workspace.
 */
#include "../../inc/dostypes.h"

#endif	/* _DOSTYPES_WRAPPER_H */
