/*
 * Data Link API - Unbind Descriptor
 */

#pragma ident "@(#)dl_unbind.c 1.1	92/10/15 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/dlpi.h>
#include <sys/errno.h>
#include <dluser.h>
#include <dlhdr.h>

extern int errno;
extern struct dl_descriptor *_getdesc();

dl_unbind(fd)
{
   struct dl_descriptor *dl;
   struct strbuf ctl;
   int flags;
   union DL_primitives prim;

   if ((dl = _getdesc(fd)) == NULL){
      errno = EBADF;
      return -1;
   }
   if (dl->info.state == DLSTATE_IDLE){
      ctl.maxlen = ctl.len = sizeof(dl_unbind_req_t);
      ctl.buf = (char *)&prim;
      memset(&prim, '\0', sizeof prim);
      prim.dl_primitive = DL_UNBIND_REQ;
   } else {
      dl->error = DLOUTSTATE;
      return -1;
   }
   if (putmsg(fd, &ctl, NULL, 0)<0){
      dl->error = DLSYSERR;
      return -1;
   }
   ctl.maxlen = sizeof prim;
   flags = 0;
   if (getmsg(fd, &ctl, NULL, &flags)<0){
      dl->error = DLSYSERR;
      return -1;
   }
   if (prim.dl_primitive != DL_OK_ACK){
      dl->error = DLOUTSTATE;
      return -1;
   }
   return 0;
}
