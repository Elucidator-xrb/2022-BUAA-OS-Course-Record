#include<stdio.h>
int main()
{
	int n;
	int n_rvs = 0;
	int tmp;
	scanf("%d",&n);
	
	tmp = n;
	while (tmp > 0) {
		n_rvs = n_rvs * 10 + tmp  % 10;
		tmp /= 10;
	}

	if(n_rvs == n){
		printf("Y");
	}else{
		printf("N");
	}
	return 0;
}
