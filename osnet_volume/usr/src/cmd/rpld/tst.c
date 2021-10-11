#pragma ident "@(#)tst.c 1.1	92/10/15 SMI"

#include "dluser.h"

unsigned char dstaddr[6] = {
00, 00, 0xc0, 0xf0, 0x78, 0x13
/*0x02, 0x07, 0x01, 0x08, 0x33, 0x97*/

};

struct dl_address *addr;
main( argc, argv )
	char *argv[];
{
   int i, fd;

   fd = dl_open(argc>1 ? argv[1] : "/dev/wd0", 2, 0);
   if (fd<0){
      perror("wd0");
      exit(1);
   }
   if (dl_bind(fd, 0x1111, 0, 0)<0){
      printf("error on bind\n");
      exit(2);
   }
   addr = dl_mkaddress(fd, dstaddr, 0x1111, 0, 0);
   printaddress(addr->dla_daddr, addr->dla_dlen);
   for (i = 0; i<10; i++)
     if (dl_snd(fd, "this is a test, only a test\n", 28, addr)<0){
	printf("an error on send\n");
     }
}

printaddress(addr, len)
     unsigned char *addr;
{
   unsigned char paddr[6];
   int sap;
   unsigned char oi[4];
   int oitype;
   int i;

   dl_parseaddr(addr, len, paddr, &sap, oi, &oitype);
   for (i=0; i<6; i++)
     printf("%02X ", paddr[i]);
   printf("%04X\n", sap);
}
