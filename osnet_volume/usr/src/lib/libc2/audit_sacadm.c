#ifdef C2_DEBUG
#define dprintf(x) {printf x;}
#else
#define dprintf(x)
#endif

/*ARGSUSED*/
void
audit_sacadm_main(argc,argv)
	int   argc;
	char *argv[];
{
#ifdef C2_DEBUG
	int i;
	printf("audit_sacadm_main(%d",argc);
	for(i=0;i<argc;i++)
		printf(",%s",argv[i]);
	printf(")\n");
#endif
}

/*ARGSUSED*/
void
audit_sacadm_main1(flag)
	int flag;
{
	dprintf(("audit_sacadm_main1(%d)\n",flag));
}

/*ARGSUSED*/
void
audit_sacadm_main2(flag)
	int flag;
{
	dprintf(("audit_sacadm_main2(%d)\n",flag));
}

/*ARGSUSED*/
void
audit_sacadm_main3(flag)
	int flag;
{
	dprintf(("audit_sacadm_main3(%d)\n",flag));
}

/*ARGSUSED*/
void
audit_sacadm_main4(flag)
	int flag;
{
	dprintf(("audit_sacadm_main4(%d)\n",flag));
}

