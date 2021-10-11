#ifdef C2_DEBUG
#define dprintf(x) {printf x;}
#else
#define dprintf(x)
#endif

/*ARGSUSED*/
void
audit_init_userinit(argc,argv)
	int   argc;
	char *argv[];
{
#ifdef C2_DEBUG
	int i;
	printf("audit_init_userinit(%d",argc);
	for(i=0;i<argc;i++)
		printf(",%s",argv[i]);
	printf(")\n");
#endif
}

void
audit_init_userinit1()
{
	dprintf(("audit_init_userinit1()\n"));
}

/*ARGSUSED*/
void
audit_init_userinit2(n)
	char *n;
{
	dprintf(("audit_init_userinit2()\n"));
}

/*ARGSUSED*/
void
audit_init_userinit3(ln,n)
	char *ln;
	char *n;
{
	dprintf(("audit_init_userinit3(%s,%s)\n",ln,n));
}

void
audit_init_userinit4()
{
	dprintf(("audit_init_userinit4()\n"));
}

void
audit_init_userinit5()
{
	dprintf(("audit_init_userinit5()\n"));
}

void
audit_init_userinit6()
{
	dprintf(("audit_init_userinit6()\n"));
}

