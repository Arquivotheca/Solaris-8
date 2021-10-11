/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)port_before.h 1.1     97/12/03 SMI"

#define __EXTENSIONS__
/* #define SVR4 */
/* #define WANT_IRS_NIS */
#undef WANT_IRS_PW 
#define SIG_FN void

#include <limits.h>	/* _POSIX_PATH_MAX */
#ifdef SUNW_SYNONYMS
#include "synonyms.h"
#endif
#ifdef SUNW_OPTIONS
#include "conf/sunoptions.h"
#endif
#include <sys/types.h>
#include "sys/bitypes.h"
#include "sys/cdefs.h"
