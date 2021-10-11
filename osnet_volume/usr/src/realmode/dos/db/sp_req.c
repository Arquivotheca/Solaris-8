/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 */
 
#pragma ident "@(#)sp_req.c 1.1       95/11/02 SMI"

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

main(argc,argv)
int argc;
char *argv[];
{

	struct stat *buf;
	char d_buf[80];
	int i,ii=1,fd,i_total,b_total=0,k_blocks,s_total=0;


	for (; ii < argc; ii++) {
		if ((fd = open(argv[ii],O_RDONLY)) < 0) {
			printf("%s: cannot open %s.\n",argv[0],argv[ii]);
			exit();
		}
		i_total=ii;
		if ((i= fstat(fd,buf)) < 0) {
			printf("%s: cannot get status of file %s.\n",argv[0],argv[ii]);
			exit();
		}

		s_total=s_total+buf->st_size;
		k_blocks=buf->st_blocks/2;
		b_total=(b_total+k_blocks);
		close(fd);
	}
	printf("%s %d %d\n",getcwd(d_buf,sizeof(d_buf)),b_total,i_total);
}
