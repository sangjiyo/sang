#include "mod.h"

static cpu_t cpus[NCPU];

cpu_t *mycpu(void)
{
    int hartid = r_tp();
    return &cpus[hartid];
}

int mycpuid(void)
{
    int hartid = r_tp();
    return hartid;
}
