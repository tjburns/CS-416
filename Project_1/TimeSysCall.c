#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int main(int argc, char *argv[]){
	
	int i;
	int numSysCalls = 100000;
	
	// time stuff
	struct timeval startTime;
	struct timeval endTime;
	double elapsedTime;
	
	gettimeofday(&startTime, 0);
	for (i = 0; i < numSysCalls; i++) {
		getpid();
	}
	gettimeofday(&endTime, 0);
	elapsedTime = 1000000*endTime.tv_sec + endTime.tv_usec - 1000000*startTime.tv_sec + startTime.tv_usec;
	
	
	printf("Syscalls Performed: %d\n", numSysCalls);
	printf("Total Elapsed Time: %f ms\n", elapsedTime);
	printf("Average Time Per Syscall: %f ms\n", elapsedTime/numSysCalls);

	return 0;

}
