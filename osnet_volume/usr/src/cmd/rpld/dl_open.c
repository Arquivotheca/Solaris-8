/*
 * Data Link API - Open/Setup
 */

#pragma ident "@(#)dl_open.c 1.2	93/12/25 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/dlpi.h>
#include <sys/errno.h>
#include <dluser.h>
#include <dlhdr.h>

static struct dl_descriptor *_dlfds;
static int _nfds;
extern int errno;

extern struct dl_address *dl_mkaddress();

dl_open(dev, mode, info)
     char *dev;
     int   mode;
     dl_info_t *info;
{
   int fd;

   if ((fd=open(dev, mode))<0){
      return -1;
   }

   ioctl(fd, I_SRDOPT, RMSGD);

   /* now have open device - do inforequest */
   if (info == NULL)
     return dl_sync(fd);
   else {
     if (dl_info(fd, info)<0)
       return -1;
     else
       return fd;
   }
}

/* this actually does the work that dl_open needs done also */
dl_sync(fd)
{
   struct strbuf ctl;
   dl_info_req_t ireq;
   dl_info_ack_t *iack;
   char buff[sizeof(dl_info_ack_t)+64];
   struct dl_descriptor *dl;
   int flags;

   /* first time? */
   if (_nfds == 0){
      _nfds = ulimit(4, 0);	/* number of possible file descriptors */
      _dlfds = (struct dl_descriptor *)malloc(_nfds*sizeof(struct dl_descriptor));
      if (_dlfds == NULL)
	return -1;
   }

   dl = &_dlfds[fd];
   dl->openflag = 1;

   /* get the info_ack data */
   ctl.maxlen=ctl.len = sizeof(ireq);
   ctl.buf = (char *)&ireq;
   ireq.dl_primitive = DL_INFO_REQ;
   if (putmsg(fd, &ctl, NULL, 0)<0){
      close(fd);
      return -1;
   }

   iack = (dl_info_ack_t *)buff;
   ctl.maxlen = sizeof(buff);
   ctl.len = 0;
   ctl.buf = (char *)iack;
   flags = 0;
   if (getmsg(fd, &ctl, NULL, &flags)<0){
      close(fd);
      return -1;
   }
   /* now have all in place */
   if (iack->dl_primitive != DL_INFO_ACK){
      close(fd);
      return -1;
   }
   /* now pick out the relevent information and fake the rest */
   dl->info.state	= iack->dl_current_state;
   dl->info.max_lsdu	= iack->dl_max_sdu;
   dl->info.min_lsdu	= iack->dl_min_sdu;
   dl->info.addr_len	= iack->dl_addr_length;
   dl->info.mac_type	= iack->dl_mac_type;
   dl->info.class	= iack->dl_service_mode;
   dl->info.style	= iack->dl_provider_style;
#if defined(DLPI_1)
   dl->info.address     = (struct dl_address *)NULL;
   dl->info.broadcast   = (struct dl_address *)NULL;
#else
   dl->info.version	= iack->dl_version;
   dl->info.sap_len     = iack->dl_sap_length;
   dl->info.address     = dl_mkaddress(fd, buff+iack->dl_addr_offset,
				       0, NULL, NULL);
   dl->info.broadcast   = dl_mkaddress(fd, buff+iack->dl_brdcst_addr_offset,
				       0, NULL, NULL);
#endif
   return fd;
}

struct dl_descriptor *
_getdesc(fd)
{
   struct dl_descriptor *dl;

   if (_nfds > 0 && fd < _nfds){
      dl = &_dlfds[fd];
      if (dl->openflag)
	return dl;
   }
   return NULL;
}

dl_error(fd)
{
   struct dl_descriptor *dl;

   if (_nfds > 0 && fd < _nfds){
      dl = &_dlfds[fd];
      if (dl->openflag){
	 return dl->error;
      }
   }
   errno = EBADF;
   return DLSYSERR;
}
