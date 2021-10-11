#include	<stdio.h>

extern void small_sub(void);

main()
{
	int i;
	for (i = 0; i < 10; i++)
		small_sub();

	printf("simple run.\n");
	return 0;
}
