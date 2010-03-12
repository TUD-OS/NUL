#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <strings.h>
#include <sys/time.h>

typedef uint32_t clock_time_t;

static clock_time_t global_system_ticks = 0;

static void
clock_tick()
{

	global_system_ticks++;
}

clock_time_t
clock_time()
{

	return (global_system_ticks);
}

unsigned long clock_seconds(void)
{

	return (global_system_ticks / 32);
}

void
clock_init()
{
	struct itimerval itv;

	signal(SIGALRM, clock_tick);
	bzero(&itv, sizeof(struct itimerval));
	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 31250; //100000;
	itv.it_value.tv_sec = 0;
	itv.it_value.tv_usec = 31250; //100000;
	setitimer(ITIMER_REAL, &itv, NULL);
	
}
