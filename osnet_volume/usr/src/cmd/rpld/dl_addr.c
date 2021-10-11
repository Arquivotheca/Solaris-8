/*
 * Data Link API - Make Address
 */

#pragma ident "@(#)dl_addr.c 1.1	92/10/15 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/dlpi.h>
#include <sys/errno.h>
#include <dluser.h>
#include <dlhdr.h>

extern int errno;
extern struct dl_descriptor *_getdesc();

struct dl_address *
dl_allocaddr(fd, which)
{
   struct dl_address *addr;
   struct dl_descriptor *dl;
   int alen;

   dl = _getdesc(fd);
   if (dl == NULL){
      errno = EBADF;
      return NULL;
   };

   addr = (struct dl_address *)malloc(sizeof(struct dl_address));
   if (addr == NULL){
      dl->error = DLSYSERR;
      return NULL;
   }

   if (which & DL_DST){
      addr->dla_dmax = 16;
      addr->dla_daddr = (unsigned char *)malloc(addr->dla_dmax);
      if (addr->dla_daddr == NULL){
	 int err;
	 err = errno;
	 free(addr);
	 errno = err;
	 dl->error = DLSYSERR;
	 return NULL;
      }
   }
   if (which & DL_SRC){
      addr->dla_smax = 16;
      addr->dla_saddr = (unsigned char *)malloc(addr->dla_smax);
      if (addr->dla_saddr == NULL){
         int err;
	 err = errno;
	 if (addr->dla_slen > 0){
	    free(addr->dla_saddr);
	 }
         free(addr);
         errno = err;
         dl->error = DLSYSERR;
         return NULL;
      }
   }
   return addr;
}

/* destination address only */

struct dl_address *
dl_mkaddress(fd, addr, sap, oi, oitype)
     unsigned char *addr;
     unsigned char *oi;
{
   struct dl_address *da;
   struct dl_descriptor *dl;

   dl = _getdesc(fd);
   if (dl == NULL)
     return NULL;

   da = dl_allocaddr(fd, DL_DST);
   if (da == NULL)
     return NULL;

   memcpy(da->dla_daddr, addr, 6);
   if (sap > 1500){
      da->dla_dlen = 8;
      *(unsigned short *)(da->dla_daddr + 6) = sap;
      return da;
   } else if (sap < 512){
	 da->dla_dlen = 7;
	 *(da->dla_daddr+6) = sap;
	 return da;
   }
   dl->error = DLBADSAP;
   return NULL;
}

#if defined(DLPI_1)
dl_parseaddr(addr, len, paddr, sap, oi, oitype)
     unsigned char *addr, *paddr, *oi;
     long *sap, *oitype;
{
   if (addr == NULL)
     return -1;
   if (paddr){
      memcpy(paddr, addr, 6);
   }
   if (sap){
      switch(len){
       case 8:
	 *sap = *(unsigned short *)(addr + 6);
	 break;
       case 7:
       case 12:
	 *sap = *(unsigned char *)(addr + 6);
	 if (*sap == 0xAA){
	    if (len != 12)
	      return -1;
	    if (oi)
	      memcpy(oi, addr + 7, 3);
	    if (oitype)
	      *oitype = *(unsigned short *)(addr + 10);
	 }
	 break;
       default:
	 return -1;
      }
   }
   return 0;
}
#else
dl_parseaddr(addr, len, paddr, sap, oi, oitype)
     unsigned char *addr, *paddr, *oi;
     long *sap, *oitype;
{
   if (addr == NULL)
     return -1;
   if (paddr){
      memcpy(paddr, addr, 6);
   }
   if (sap){
      switch(len){
       case 8:
	 *sap = *(unsigned short *)(addr + 6);
	 break;
       case 7:
       case 12:
	 *sap = *(unsigned char *)(addr + 6);
	 if (*sap == 0xAA){
	    if (len != 12)
	      return -1;
	    if (oi)
	      memcpy(oi, addr + 7, 3);
	    if (oitype)
	      *oitype = *(unsigned short *)(addr + 10);
	 }
	 break;
       default:
	 return -1;
      }
   }
   return 0;
}
#endif
