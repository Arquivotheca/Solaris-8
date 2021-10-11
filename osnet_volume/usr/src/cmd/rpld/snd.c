#pragma ident "@(#)snd.c 1.1	92/10/15 SMI"

#include <stdio.h>
#include "dluser.h"

unsigned char dest[6] = {0,0,0xc0, 0x21, 0x8f, 8};
struct dl_address *addr;
main( argc, argv )
	char *argv[];
{
   int i, fd;

   printf("opening %s\n", argc > 1 ? argv[1] : "/dev/le", 2, 0);
   fd = dl_open(argc>1 ? argv[1] : "/dev/le", 2, 0);
   if (fd<0){
      perror("wd0");
      exit(1);
   }
   if (dl_attach(fd, 3)<0){
	perror("error on dl_attach\n");
	exit(10);
   }
   if (dl_bind(fd, 0x40, 0, 0)<0){
      perror("error on bind\n");
      printf("dl error = %d\n", dl_error(fd));
      exit(2);
   }
   addr = dl_mkaddress(fd, dest, 0x40, NULL, NULL);


   for(i=0;;i++){
      char buff[2048];
      int len;
      for (len=0; len<1024; len++)
	buff[len] = i;
      len = 1024;
      if (dl_snd(fd, buff, len, addr, 0)<0){
	 printf("an error on send (%d)\n", dl_error(fd));
	 exit(1);
      }
      printf("sent packet %d\n", i);
      sleep(1);
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

   printf("alen=%d",len);
   for (i=0; i<len; i++)printf(" %02X", addr[i]);printf("\n");
   dl_parseaddr(addr, len, paddr, &sap, oi, &oitype);
   for (i=0; i<6; i++)
     printf("%02X ", paddr[i]);
   printf("%04X\n", sap);
}
