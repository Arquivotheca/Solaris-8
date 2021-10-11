/*
 * Data Link API - Get info for user
 */

#pragma ident "@(#)dl_info.c 1.1	92/10/15 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/dlpi.h>
#include <dluser.h>
#include <dlhdr.h>

extern struct dl_descriptor *_getdesc();
extern int errno;

dl_info(fd, info)
     int fd;
     dl_info_t *info;
{
   struct dl_descriptor *dl;
   struct strbuf ctl;
   int flags;

   dl = _getdesc(fd);
   if (dl != NULL){
     if (dl_sync(fd)<0)
       return -1;
     if (info != NULL)
       *info = dl->info;
     return 0;
   }
   return -1;
}
