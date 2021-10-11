/*
 * Data Link API - Receive Data
 */

#pragma ident "@(#)dl_rcv.c 1.1	92/10/15 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/dlpi.h>
#include <dluser.h>
#include <dlhdr.h>

extern struct dl_descriptor *_getdesc();
extern int errno;

dl_rcv(fd, buff, bufflen, addr)
     char *buff;
     int *bufflen;
     struct dl_address *addr;
{
   struct dl_descriptor *dl;
   struct strbuf ctl, data;
   int flags = 0;

   if ((dl = _getdesc(fd)) != NULL){
      unsigned char pbuff[sizeof(union DL_primitives)+24];
      union DL_primitives *prim = (union DL_primitives *)pbuff;
      int len;
      unsigned char *ap, *addrp;

      if (dl->info.state != DLSTATE_IDLE){
         dl->error = DLUNBOUND;
         return -1;
      }
      memset(prim, '\0', sizeof prim);
      ctl.maxlen = ctl.len = sizeof pbuff;
      ctl.buf = (char *)prim;
      data.maxlen = data.len = *bufflen;
      data.buf = buff;
      flags = 0;
      if (getmsg(fd, &ctl, &data, &flags)<0){
	 dl->error = DLSYSERR;
	 return -1;
      }
      if (ctl.len == 0){
				/* not really supported but ok */
	 *bufflen = 0;
	 return 0;
      }
      switch (prim->dl_primitive){
       case DL_UNITDATA_IND:
	 if (addr->dla_dmax > 0){
	    for (addr->dla_dlen=len=prim->unitdata_ind.dl_dest_addr_length,
		 addrp = addr->dla_daddr,
		 ap = ((unsigned char *)prim)+prim->unitdata_ind.dl_dest_addr_offset;
		 len > 0; len--)
	      *addrp++ = *ap++;
#if !defined(DLPI_1)
	    addr->dla_dflag = prim->unitdata_ind.dl_group_address;
#endif
	 }
	 if (addr->dla_smax > 0){
	    for (addr->dla_slen=len=prim->unitdata_ind.dl_src_addr_length,
		 addrp = addr->dla_saddr,
		 ap = ((unsigned char *)prim)+prim->unitdata_ind.dl_src_addr_offset;
		 len > 0; len--)
	      *addrp++ = *ap++;
	 }
	 *bufflen = data.len;
	 break;
       default:
	 dl->error = DLOUTSTATE;
	 return -1;
      }
      return 0;
   }
   return -1;
}
