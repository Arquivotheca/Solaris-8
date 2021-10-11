/*
 * Data Link API - Close Descriptor
 */

#pragma ident "@(#)dl_close.c 1.1	92/10/15 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/dlpi.h>
#include <sys/errno.h>
#include <dluser.h>
#include <dlhdr.h>

extern int errno;
extern struct dl_descriptor *_getdesc();

dl_close(fd)
{
   struct dl_descriptor *dl;

   if ((dl = _getdesc(fd)) == NULL){
      errno = EBADF;
      return -1;
   }
   dl->openflag = 0;
   return close(fd);
}
