#include "hazard.h"
#include <stddef.h>

void hazard_init(HazardManager *hm)
{
    for (int i = 0; i < MAX_THREADS; i++) {
        atomic_store(&hm->active[i], 0);

        for (int j = 0; j < MAX_HAZARDS; j++)
            atomic_store(&hm->slots[i][j], NULL);
    }
}

int hazard_register(HazardManager *hm)
{
    for (int i = 0; i < MAX_THREADS; i++) {
        int expected = 0;

        if (atomic_compare_exchange_strong(
                &hm->active[i], &expected, 1))
            return i;
    }

    return -1;
}

void hazard_set(HazardManager *hm, int tid, int slot, void *ptr)
{
    atomic_store_explicit(
        &hm->slots[tid][slot],
        ptr,
        memory_order_seq_cst);
}

void hazard_clear(HazardManager *hm, int tid, int slot)
{
    atomic_store_explicit(
        &hm->slots[tid][slot],
        NULL,
        memory_order_seq_cst);
}

int hazard_is_protected(HazardManager *hm, void *ptr)
{
    for (int i = 0; i < MAX_THREADS; i++) {
        for (int j = 0; j < MAX_HAZARDS; j++) {
            if (atomic_load_explicit(
                    &hm->slots[i][j],
                    memory_order_seq_cst) == ptr)
                return 1;
        }
    }

    return 0;
}
