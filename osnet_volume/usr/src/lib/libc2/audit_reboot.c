#ifdef C2_DEBUG
#define dprintf(x) {printf x;}
#else
#define dprintf(x)
#endif

/*ARGSUSED*/
void
audit_reboot_main(argc,argv)
	int   argc;
	char *argv[];
{
#ifdef C2_DEBUG
	int i;
	printf("audit_reboot_main(%d",argc);
	for(i=0;i<argc;i++)
		printf(",%s",argv[i]);
	printf(")\n");
#endif
}

/*ARGSUSED*/
void
audit_reboot_main1(user)
	char *user;
{
	dprintf(("audit_reboot_main1(%s)\n",(user)?user:"?"));
}

void
audit_reboot_main2()
{
	dprintf(("audit_reboot_main2()\n"));
}

void
audit_reboot_reboot()
{
	dprintf(("audit_reboot_reboot()\n"));
}

/*ARGSUSED*/
void
audit_reboot_reboot1(howto)
	int howto;
{
	dprintf(("audit_reboot_reboot1(%d)\n",howto));
}

/*ARGSUSED*/
void
audit_reboot_success()
{
	dprintf("audit_reboot_success()\n");
}
