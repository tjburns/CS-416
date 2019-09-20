#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>

static int exceptions = 0;
// time stuff
struct timeval startTime;
struct timeval endTime;
double elapsedTime;

void handle_sigfpe(int signum){
	//printf("divide by zero error\n");
	exceptions++;
	
	if (exceptions == 100000) {
		gettimeofday(&endTime, 0);
		elapsedTime = 1000000*endTime.tv_sec + endTime.tv_usec - 1000000*startTime.tv_sec + startTime.tv_usec;
	
		printf("Exceptions Occured: %d\n", exceptions);
		printf("Total Elapsed Time: %f ms\n", elapsedTime);
		printf("Average Time Per Exception: %f ms\n", elapsedTime/exceptions);
		
		exit(0);
	}
}

int main(int argc, char *argv[]){
	
	int x = 2;
	int y = 0;
	int z = 0;
	
	signal(SIGFPE, handle_sigfpe);
	
	gettimeofday(&startTime, 0);
	z = x / y;
	
	return 0;
}
