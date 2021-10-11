#ifdef C2_DEBUG
#define dprintf(x) {printf x;}
#else
#define dprintf(x)
#endif

/*ARGSUSED*/
void
audit_ncheck_main(argc,argv)
	int   argc;
	char *argv[];
{
#ifdef C2_DEBUG
	int i;
	printf("audit_ncheck_main(%d",argc);
	for(i=0;i<argc;i++)
		printf(",%s",argv[i]);
	printf(")\n");
#endif
}
