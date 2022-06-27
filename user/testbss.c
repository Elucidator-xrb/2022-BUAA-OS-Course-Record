#include "lib.h"
#define ARRAYSIZE (1024*5)

int bigarray[ARRAYSIZE];

void umain(int argc, char **argv) {
	int i;

	writef("Making sure bss works right...\n");
	for(i = 0;i < ARRAYSIZE; i++)
		if(bigarray[i]!=0)
			user_panic("bigarray[%d] isn't cleared!\n",i);
	for (i = 0; i < ARRAYSIZE; i++)
		bigarray[i] = i;
	for (i = 0; i < ARRAYSIZE; i++)
		if (bigarray[i] != i)
			user_panic("bigarray[%d] didn't hold its value!\n", i);

	writef("Yes, good. Now doing a wild write off the end...\n");
	bigarray[ARRAYSIZE+1024] = 0;
	user_panic("SHOULD HAVE TRAPPED!!!");
}