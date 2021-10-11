/*
 * Data Link API - Attach physical unit
 */

#pragma ident "@(#)dl_attach.c 1.1	92/10/15 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/dlpi.h>
#include <dluser.h>
#include <dlhdr.h>

extern struct dl_descriptor *_getdesc();
extern int errno;

dl_attach(fd, unit)
     int fd, unit;
{
   struct dl_descriptor *dl;
   struct strbuf ctl;
   int flags;

   dl = _getdesc(fd);
   if (dl != NULL){
      union DL_primitives prim;

      if (dl->info.state != DL_UNATTACHED){
	if (dl->info.style != DL_STYLE_2)
	  dl->error = DLNOTSUPP;
	else if (dl->info.state == DL_BOUND)
	 dl->error = DLBOUND;
	else dl->error = DLSYSERR;
	 return -1;
      }

      prim.dl_primitive = DL_ATTACH_REQ;
      prim.attach_req.dl_ppa = unit; /* this is opaque data of 32-bits */
      ctl.maxlen=ctl.len = sizeof(prim);
      ctl.len=ctl.len = sizeof(prim.attach_req);
      ctl.buf = (char *)&prim;
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
      switch (prim.dl_primitive){
       case DL_ERROR_ACK:
	 dl->error = prim.error_ack.dl_errno;
	 errno = prim.error_ack.dl_unix_errno;
	 return -1;
       case DL_OK_ACK:
	 dl->info.state = DLSTATE_UNBOUND;
	 return 0;
       default:
	 dl->error = DLBADPRIM;
	 return -1;
      }
   }
   return -1;
}
