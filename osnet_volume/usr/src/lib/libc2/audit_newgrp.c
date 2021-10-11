#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#ifdef C2_DEBUG
#define dprintf(x) {printf x;}
#else
#define dprintf(x)
#endif

/*ARGSUSED*/
void
audit_newgrp_main(argc,argv)
	int   argc;
	char *argv[];
{
#ifdef C2_DEBUG
	int i;
	printf("audit_newgrp_main(%d",argc);
	for(i=0;i<argc;i++)
		printf(",%s",argv[i]);
	printf(")\n");
#endif
}

void
audit_newgrp_main1()
{
	dprintf(("audit_newgrp_main1()\n"));
}

/*ARGSUSED*/
void
audit_newgrp_main2(p)
	struct passwd *p;
{
	dprintf(("audit_newgrp_main1(%x)\n",p));
}

/*ARGSUSED*/
void
audit_newgrp_chkgrp(g)
	struct group *g;
{
	dprintf(("audit_newgrp_chkgrp(%x)\n",g));
}

void
audit_newgrp_chkgrp1()
{
	dprintf(("audit_newgrp_chkgrp1()\n"));
}

