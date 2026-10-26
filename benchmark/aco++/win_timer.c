/*
 * Windows-portable replacement for unix_timer.c (MinGW has no sys/resource.h).
 * Semantics preserved: elapsed_time(VIRTUAL) == CPU time (user+system),
 * matching getrusage(RUSAGE_SELF) ru_utime+ru_stime. REAL is never used by
 * this codebase, so both map to CPU time.
 */
#include <stdio.h>
#include <time.h>

#include "timer.h"

static clock_t cpu_start;

void start_timers(void)
{
    cpu_start = clock();
}

double elapsed_time(TIMER_TYPE type)
{
    (void) type; /* REAL unused */
    return (double)(clock() - cpu_start) / (double)CLOCKS_PER_SEC;
}
