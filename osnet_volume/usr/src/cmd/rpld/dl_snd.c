/*
 * Data Link API - Send Data
 */

#pragma ident "@(#)dl_snd.c 1.1	92/10/15 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/dlpi.h>
#include <dluser.h>
#include <dlhdr.h>

extern struct dl_descriptor *_getdesc();
extern int errno;

#if defined(DLPI_1)
dl_snd(fd, buff, bufflen, addr)
     char *buff;
     struct dl_address *addr;
#else
dl_snd(fd, buff, bufflen, addr, priority)
     char *buff;
     struct dl_address *addr;
     dl_priority_t *priority;
#endif
{
   struct dl_descriptor *dl;
   struct strbuf ctl, data;

   if ((dl = _getdesc(fd)) != NULL){
      union DL_primitives prim;

      if (dl->info.state != DLSTATE_IDLE){
         dl->error = DLUNBOUND;
         return -1;
      }
      memset(&prim, '\0', sizeof prim);
      prim.unitdata_req.dl_primitive = DL_UNITDATA_REQ;
#if !defined(DLPI_1)
      if (priority != NULL)
	prim.unitdata_req.dl_priority = *priority;
#endif
      prim.unitdata_req.dl_dest_addr_length = addr->dla_dlen;
      prim.unitdata_req.dl_dest_addr_offset = sizeof(dl_unitdata_req_t);
      memcpy(((unsigned char *)&prim)+sizeof(dl_unitdata_req_t),
	     addr->dla_daddr, addr->dla_dlen);
      ctl.maxlen = ctl.len = sizeof(dl_unitdata_req_t) + addr->dla_dlen;
      ctl.buf = (char *)&prim;
      data.maxlen = data.len = bufflen;
      data.buf = buff;
      if (putmsg(fd, &ctl, &data, 0)<0){
	 dl->error = DLSYSERR;
	 return -1;
      }
      return 0;
   }
   return -1;
}
