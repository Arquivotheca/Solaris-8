#pragma ident "@(#)getaddr.c 1.1	92/10/15 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/wd.h>

main(argc, argv)
     char *argv[];
{
  struct strioctl ioc;
  unsigned char oldaddr[6];
#if defined(DLPI_1)
   int fd = open(argc>1 ? argv[1] : "/dev/wd0", 2);
   if (fd < 0){
	perror("open");
	exit(1);
   }
   ioc.ic_cmd = DLGADDR;
   ioc.ic_dp = oldaddr;
   ioc.ic_len = 6;
   if (ioctl(fd, I_STR, &ioc)<0){
      perror("DLGADDR");
   }
   printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
	  oldaddr[0],
	  oldaddr[1],
	  oldaddr[2],
	  oldaddr[3],
	  oldaddr[4],
	  oldaddr[5]);
#else
  dl_info_t info;
  unsigned char *oaddr;
  fd = dl_open(argc>1 ? argv[1] : "/dev/wd0", 2, &info);
  oaddr = info->
#endif
   printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
	  oldaddr[0],
	  oldaddr[1],
	  oldaddr[2],
	  oldaddr[3],
	  oldaddr[4],
	  oldaddr[5]);
}
