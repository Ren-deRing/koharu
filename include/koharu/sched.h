#pragma once

#include <koharu/thread.h>

#define TICK_HZ 100
#define TIMER_VECTOR 0x20
#define MLFQ_LEVELS 4
#define BOOST_PERIOD 50

static const int QUANTUM_TICKS[MLFQ_LEVELS] = {1, 2, 4, 8};

void sched_yield(void);
void sched_tick(void);
void sched_boot(void);
void sched_enqueue(struct thread *t);