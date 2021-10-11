#ifdef C2_DEBUG
#define dprintf(x) {printf x;}
#else
#define dprintf(x)
#endif

/*ARGSUSED*/
void
audit_accton_main(argc,argv)
	int   argc;
	char *argv[];
{
#ifdef C2_DEBUG
	int i;
	printf("audit_accton_main(%d",argc);
	for(i=0;i<argc;i++)
		printf(",%s",argv[i]);
	printf(")\n");
#endif
}

void
audit_accton_main1()
{
	dprintf(("audit_accton_main1()\n"));
}

void
audit_accton_main2()
{
	dprintf(("audit_accton_main2()\n"));
}

void
audit_accton_main3()
{
	dprintf(("audit_accton_main3()\n"));
}
 
void
audit_accton_main4()
{
	dprintf(("audit_accton_main4()\n"));
}
 
void
audit_accton_main5()
{
	dprintf(("audit_accton_main5()\n"));
}
 
/*ARGSUSED*/
void
audit_accton_ckfile(admfile)
	char *admfile;
{
	dprintf(("audit_accton_ckfile(%s)\n",admfile));
}
 
/*ARGSUSED*/
void
audit_accton_ckfile1(admfile)
	char *admfile;
{
	dprintf(("audit_accton_ckfile1(%s)\n",admfile));
}
 
/*ARGSUSED*/
void
audit_accton_ckfile2(admfile)
	char *admfile;
{
	dprintf(("audit_accton_main1(%s)\n",admfile));
}
 
