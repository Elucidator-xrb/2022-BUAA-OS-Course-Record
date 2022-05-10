#include <env.h>
#include <pmap.h>
#include <printf.h>

/* Overview:
 *  Implement simple round-robin scheduling.
 *
 *
 * Hints:
 *  1. The variable which is for counting should be defined as 'static'.
 *  2. Use variable 'env_sched_list', which is a pointer array.
 *  3. CANNOT use `return` statement!
 */
/*** exercise 3.15 ***/
extern struct Env_list env_sched_list[2];
extern struct Env *curenv;

void print_cur_sched_list() {
	printf("******* show env_sched_list ********\n");
	struct Env *e = NULL;
	int i;
	for (i = 0; i < 2; ++i) {
		printf("## env_sched_list[%d] ##\n", i);
		LIST_FOREACH(e, &env_sched_list[i], env_sched_link) {
			printf("env_id:%x, env_pri:%d\n", e->env_id, e->env_pri);
		}
	}
	printf("******* end  env_sched_list *******\n ");
}

void sched_yield(void)
{
    static int count = 0; // remaining time slices of current env
    static int point = 0; // current env_sched_list index
    
    /*  hint:
     *  1. if (count==0), insert `e` into `env_sched_list[1-point]`
     *     using LIST_REMOVE and LIST_INSERT_TAIL.
     *  2. if (env_sched_list[point] is empty), point = 1 - point;
     *     then search through `env_sched_list[point]` for a runnable env `e`, 
     *     and set count = e->env_pri
     *  3. count--
     *  4. env_run()
     *
     *  functions or macros below may be used (not all):
     *  LIST_INSERT_TAIL, LIST_REMOVE, LIST_FIRST, LIST_EMPTY
     */
	struct Env *e = curenv;
//printf("Yield Begin: count = %d, point = %d\n", count, point);
//print_cur_sched_list();

	if (count == 0 || curenv == NULL || curenv->env_status != ENV_RUNNABLE) {
//printf("Switch Env Begin\n");
		if (curenv != NULL)
			LIST_INSERT_TAIL(&env_sched_list[1-point], curenv, env_sched_link);
		
		while (1) { //printf("\tIn Loop\n");
			if (LIST_EMPTY(&env_sched_list[point])) point = 1 - point;
			if (LIST_EMPTY(&env_sched_list[point])) panic("sched_empty!");

			e = LIST_FIRST(&env_sched_list[point]);
			LIST_REMOVE(e, env_sched_link);
			if (e->env_status == ENV_RUNNABLE) break;
			LIST_INSERT_TAIL(&env_sched_list[1-point], e, env_sched_link);
		}
		count = e->env_pri;
//printf("Switch Env End. count = %d, point = %d\n", count, point);
	}
	count --;
//printf("Yield End, Next Run. count = %d, point = %d\n", count, point);
	env_run(e);
	//env_run(LIST_FIRST(env_sched_list));
}


