#include "sched.h"
#include <string.h>

#define MAX(x, y) (((x) >= (y)) ? (x) : (y))

void FCFS(int n, const int st[], const int rt[], int ss[]) {
    int i, last_finish_time = 0;
    for (i = 0; i < n; ++i) {
        ss[i] = MAX(st[i], last_finish_time);
        last_finish_time = ss[i] + rt[i];
    }
}

int  t[2000];
char va[2000];

void SJF(int n, const int st[], const int rt[], int ss[]) {
    memset(t , 0, n * sizeof(int));
    memset(va, 1, n);

    int i, j, cur;
    int last_finish_time = 0;
    int chosen, min_rt;

    for (i = 0, cur = 0; i < n; ++i) { //printf("I'm in %d\n", i);
        while (!va[cur]) cur++;
        chosen = cur;
        min_rt = rt[chosen];
        for (j = cur + 1; j < n; ++j) {
            if (st[j] > last_finish_time && st[j] != st[chosen]) break;
            if (va[j] && rt[j] < min_rt) {
                min_rt = rt[j];
                chosen = j;
            }
        }

        t[i] = MAX(st[chosen], last_finish_time);
        ss[chosen] = t[i];
        va[chosen] = 0; //printf("ss[%d]: %d\n", chosen, ss[chosen]);
        last_finish_time = t[i] + rt[chosen];
    }
}
