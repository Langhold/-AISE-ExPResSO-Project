#ifndef __EXPRESSO_H
#define __EXPRESSO_H

#include <stdlib.h>
#include <stdio.h>
#include "pthread.h"

typedef enum {
  SCHEDULE_DYNAMIC,
  SCHEDULE_STATIC,
  SCHEDULE_BALANCED
} schedule_t;

typedef struct {
	void (* work) (void *);
	void * data;
	void * next;
} work_t;

int expresso_initialize(void);

size_t expresso_worker_count(void);

schedule_t expresso_schedule_get(void);

void expresso_schedule_set(schedule_t);

int expresso_task(void (*) (void *), void *);

int expresso_weighted_task(void (*) (void *), void *, unsigned int);

int expresso_wait(void);

void expresso_stats(void);

double expresso_wall_time(void);

int expresso_finalize(void);

#endif
