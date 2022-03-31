#include <asm/asm.h>
#include <pmap.h>
#include <env.h>
#include <printf.h>
#include <kclock.h>
#include <trap.h>


void mips_init()
{
	printf("init.c:\tmips_init() is called\n");
	
	/*----- printf test -start -------*/
//	printf("test %%:{ %\n;%\\n  }\n");
//	printf("test %%s:{ %s;\%s }\n", "ab", "cd");
	/*----- printf test -end  ------- */

	//for your degree,don't delete these.
	//------------|
	#ifdef FTEST
	FTEST();
	#endif

	#ifdef PTEST
	ENV_CREATE(PTEST);
	#endif
	//-----------|
	panic("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
}
