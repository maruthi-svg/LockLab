#ifndef HAZARD_H
#define HAZARD_H

#include <stdatomic.h>

#define MAX_THREADS 16
#define MAX_HAZARDS 2

typedef struct {
    _Atomic(void *) slots[MAX_THREADS][MAX_HAZARDS];
    _Atomic int active[MAX_THREADS];
} HazardManager;

void hazard_init(HazardManager *hm);
int hazard_register(HazardManager *hm);
void hazard_set(HazardManager *hm, int tid, int slot, void *ptr);
void hazard_clear(HazardManager *hm, int tid, int slot);
int hazard_is_protected(HazardManager *hm, void *ptr);

#endif
