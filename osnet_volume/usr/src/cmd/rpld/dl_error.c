/*
 * Data Link API - error routines
 */

#pragma ident "@(#)dl_error.c 1.1	92/10/15 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/dlpi.h>
#include <dluser.h>
#include <dlhdr.h>

extern struct dl_descriptor *_getdesc();
extern int errno;

dl_errno(fd)
{
   struct dl_descriptor *dl;

   dl = _getdesc(fd);
   if (dl != NULL){
	return dl->error;
   }
   return  -1;
}
