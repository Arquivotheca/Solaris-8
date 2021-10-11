#ifdef C2_DEBUG
#define dprintf(x) {printf x;}
#else
#define dprintf(x)
#endif

/*ARGSUSED*/
void
audit_halt_main(argc,argv)
	int   argc;
	char *argv[];
{
#ifdef C2_DEBUG
	int i;
	printf("audit_halt_main(%d",argc);
	for(i=0;i<argc;i++)
		printf(",%s",argv[i]);
	printf(")\n");
#endif
}

/*ARGSUSED*/
void
audit_halt_main1(ttyn)
	char *ttyn;
{
	dprintf(("audit_halt_main1(%s)\n",(ttyn)?ttyn:"0"));
}

/*ARGSUSED*/
void
audit_halt_main2(user)
	char *user;
{
	dprintf(("audit_halt_main2(%s)\n",(user)?user:"?"));
}

void
audit_halt_main3()
{
	dprintf(("audit_halt_main3()\n"));
}

void
audit_halt_reboot()
{
	dprintf(("audit_halt_reboot()\n"));
}

void
audit_halt_reboot1()
{
	dprintf(("audit_halt_reboot1()\n"));
}

void
audit_halt_success()
{
	dprintf(("audit_halt_success()\n"));
}

