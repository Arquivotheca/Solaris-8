/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)devio.c	1.10	99/01/31 SMI\n"

/*
 * Device interface code for standalone I/O system.
 *
 */

#ifdef DEBUG
    #pragma message ( __FILE__ ": << WARNING! DEBUG MODE >>" )
    #pragma comment ( user, __FILE__ ": DEBUG ON " __TIMESTAMP__ )
#endif

#pragma comment ( compiler )
#pragma comment ( user, "devio.c	1.10	99/01/31" )


#include <sys\types.h>
#include <sys\param.h>
#include <sys\saio.h>
#include <sys\fs\ufs_fs.h>
#include <bioserv.h>          /* BIOS interface support routines */
#include <dev_info.h>         /* MDB extended device information */
#include <bootp2s.h>          /* primary/secondary boot interface */
#include <bootdefs.h>         /* primary boot environment values */

extern struct pri_to_secboot *realp;
extern struct bios_dev boot_dev;

long
devread(daddr_t adjusted_blk, char far *Buffer, short nsect)
{
	short rc;

	rc = read_sectors(&boot_dev, adjusted_blk, nsect, Buffer);

	return ((rc == 0) ? -1 : (rc * realp->bootfrom.ufs.bytPerSec));
}
