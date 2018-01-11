#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <counters_features.h>
#include <clocks_features.h>
#include <config_architecture.h>
#include <msr_core.h>
#include <power_features.h>
#include <thermal_features.h>
#include <variorum.h>
#include <variorum_cpuid.h>
#include <variorum_error.h>

#define BILLION 1000000000L
#define CHUNK_SIZE 100000

int cpuid_num_pmc(void)
{
    /* See Manual Vol 3B, Section 18.2.5 for details. */
    uint64_t rax, rbx, rcx, rdx;
    int leaf = 10; // 0A

    cpuid(leaf, &rax, &rbx, &rcx, &rdx);
    // If CPUID.0AH:rax[15:8] > 3, then up to PMC3 is usable
    // If CPUID.0AH:rax[15:8] > 2, then up to PMC2 is usable
    // If CPUID.0AH:rax[15:8] > 1, then up to PMC1 is usable
    // If CPUID.0AH:rax[15:8] > 0, then only PMC0 is usable
    // If CPUID.0AH:rax[15:8] == 0, then none are usable
    return MASK_VAL(rax, 15, 8);
}

/*****************************************/
/* Fixed Counters Performance Monitoring */
/*****************************************/

void fixed_counter_storage(struct fixed_counter **ctr0, struct fixed_counter **ctr1, struct fixed_counter **ctr2, off_t *msrs_fixed_ctrs)
{
    static struct fixed_counter c0, c1, c2;
    static int init = 0;
    static int nthreads;

    if (!init)
    {
        init = 1;
        variorum_set_topology(NULL, NULL, &nthreads);
        init_fixed_counter(&c0);
        init_fixed_counter(&c1);
        init_fixed_counter(&c2);
        allocate_batch(FIXED_COUNTERS_DATA, 3UL * nthreads);
        load_thread_batch(msrs_fixed_ctrs[0], c0.value, FIXED_COUNTERS_DATA);
        load_thread_batch(msrs_fixed_ctrs[1], c1.value, FIXED_COUNTERS_DATA);
        load_thread_batch(msrs_fixed_ctrs[2], c2.value, FIXED_COUNTERS_DATA);
    }
    if (ctr0 != NULL)
    {
        *ctr0 = &c0;
    }
    if (ctr1 != NULL)
    {
        *ctr1 = &c1;
    }
    if (ctr2 != NULL)
    {
        *ctr2 = &c2;
    }
}

void init_fixed_counter(struct fixed_counter *ctr)
{
    int nthreads = 0;
    variorum_set_topology(NULL, NULL, &nthreads);

    ctr->enable = (uint64_t *) malloc(nthreads * sizeof(uint64_t));
#ifdef VARIORUM_DEBUG
    fprintf(stderr, "DEBUG: note q1, ctr->enable is at %p\n", ctr->enable);
#endif
    ctr->ring_level = (uint64_t *) malloc(nthreads * sizeof(uint64_t));
    ctr->anyThread = (uint64_t *) malloc(nthreads * sizeof(uint64_t));
    ctr->pmi = (uint64_t *) malloc(nthreads * sizeof(uint64_t));
    ctr->overflow = (uint64_t *) malloc(nthreads * sizeof(uint64_t));
    ctr->value = (uint64_t **) malloc(nthreads * sizeof(uint64_t *));
}

void enable_fixed_counters(off_t *msrs_fixed_ctrs, off_t msr1, off_t msr2)
{
    struct fixed_counter *c0, *c1, *c2;
    int i;
    int nthreads = 0;

    variorum_set_topology(NULL, NULL, &nthreads);
    fixed_counter_storage(&c0, &c1, &c2, msrs_fixed_ctrs);

    for (i = 0; i < nthreads; i++)
    {
        c0->enable[i] = c1->enable[i] = c2->enable[i] = 1;
        c0->ring_level[i] = c1->ring_level[i] = c2->ring_level[i] = 3; // usr + os
        c0->anyThread[i] = c1->anyThread[i] = c2->anyThread[i] = 1;
        c0->pmi[i] = c1->pmi[i] = c2->pmi[i] = 0;
    }
    set_fixed_counter_ctrl(c0, c1, c2, msr1, msr2);
}

void disable_fixed_counters(off_t *msrs_fixed_ctrs, off_t msr1, off_t msr2)
{
    struct fixed_counter *c0, *c1, *c2;
    int i;
    int nthreads = 0;

    variorum_set_topology(NULL, NULL, &nthreads);
    fixed_counter_storage(&c0, &c1, &c2, msrs_fixed_ctrs);

    for (i = 0; i < nthreads; i++)
    {
        c0->enable[i] = c1->enable[i] = c2->enable[i] = 0;
        c0->ring_level[i] = c1->ring_level[i] = c2->ring_level[i] = 3; // usr + os
        c0->anyThread[i] = c1->anyThread[i] = c2->anyThread[i] = 1;
        c0->pmi[i] = c1->pmi[i] = c2->pmi[i] = 0;
    }
    set_fixed_counter_ctrl(c0, c1, c2, msr1, msr2);
}

void set_fixed_counter_ctrl(struct fixed_counter *ctr0, struct fixed_counter *ctr1, struct fixed_counter *ctr2, off_t msr1, off_t msr2)
{
    static uint64_t **perf_global_ctrl = NULL;
    static uint64_t **fixed_ctr_ctrl = NULL;
    static int init = 0;
    int i;
    int nthreads = 0;

    if (!init)
    {
        fixed_counter_ctrl_storage(&perf_global_ctrl, &fixed_ctr_ctrl, msr1, msr2);
        init = 1;
    }

    variorum_set_topology(NULL, NULL, &nthreads);

    /* Don't need to read counters data, we are just zeroing things out. */
    read_batch(FIXED_COUNTERS_CTRL_DATA);

    for (i = 0; i < nthreads; i++)
    {
        *ctr0->value[i] = 0;
        *ctr1->value[i] = 0;
        *ctr2->value[i] = 0;
        *perf_global_ctrl[i] = (*perf_global_ctrl[i] & ~(1ULL<<32)) | ctr0->enable[i] << 32;
        *perf_global_ctrl[i] = (*perf_global_ctrl[i] & ~(1ULL<<33)) | ctr1->enable[i] << 33;
        *perf_global_ctrl[i] = (*perf_global_ctrl[i] & ~(1ULL<<34)) | ctr2->enable[i] << 34;

        *fixed_ctr_ctrl[i] = (*fixed_ctr_ctrl[i] & (~(3ULL<<0))) | (ctr0->ring_level[i] << 0);
        *fixed_ctr_ctrl[i] = (*fixed_ctr_ctrl[i] & (~(1ULL<<2))) | (ctr0->anyThread[i] << 2);
        *fixed_ctr_ctrl[i] = (*fixed_ctr_ctrl[i] & (~(1ULL<<3))) | (ctr0->pmi[i] << 3);

        *fixed_ctr_ctrl[i] = (*fixed_ctr_ctrl[i] & (~(3ULL<<4))) | (ctr1->ring_level[i] << 4);
        *fixed_ctr_ctrl[i] = (*fixed_ctr_ctrl[i] & (~(1ULL<<6))) | (ctr1->anyThread[i] << 6);
        *fixed_ctr_ctrl[i] = (*fixed_ctr_ctrl[i] & (~(1ULL<<7))) | (ctr1->pmi[i] << 7);

        *fixed_ctr_ctrl[i] = (*fixed_ctr_ctrl[i] & (~(3ULL<<8)))  | (ctr2->ring_level[i] << 8);
        *fixed_ctr_ctrl[i] = (*fixed_ctr_ctrl[i] & (~(1ULL<<10))) | (ctr2->anyThread[i] << 10);
        *fixed_ctr_ctrl[i] = (*fixed_ctr_ctrl[i] & (~(1ULL<<11))) | (ctr2->pmi[i] << 11);
    }
    write_batch(FIXED_COUNTERS_CTRL_DATA);
    //write_batch(FIXED_COUNTERS_DATA);
}

void fixed_counter_ctrl_storage(uint64_t ***perf_ctrl, uint64_t ***fixed_ctrl, off_t msr_perf_global_ctrl, off_t msr_fixed_counter_ctrl)
{
    static uint64_t **perf_global_ctrl = NULL;
    static uint64_t **fixed_ctr_ctrl = NULL;
    static int nthreads = 0;
    static int init = 0;

    if (!init)
    {
        variorum_set_topology(NULL, NULL, &nthreads);
        perf_global_ctrl = (uint64_t **) malloc(nthreads * sizeof(uint64_t *));
        fixed_ctr_ctrl = (uint64_t **) malloc(nthreads * sizeof(uint64_t *));
        allocate_batch(FIXED_COUNTERS_CTRL_DATA, 2UL * nthreads);
        load_thread_batch(msr_perf_global_ctrl, perf_global_ctrl, FIXED_COUNTERS_CTRL_DATA);
        load_thread_batch(msr_fixed_counter_ctrl, fixed_ctr_ctrl, FIXED_COUNTERS_CTRL_DATA);
        init = 1;
    }
    if (perf_ctrl != NULL)
    {
        *perf_ctrl = perf_global_ctrl;
    }
    if (fixed_ctrl != NULL)
    {
        *fixed_ctrl = fixed_ctr_ctrl;
    }
}

void dump_all_counter_data(FILE *writedest, off_t *msrs_fixed_ctrs, off_t msr_perf_global_ctrl, off_t msr_fixed_counter_ctrl, off_t *msrs_perfevtsel_ctrs, off_t *msrs_perfmon_ctrs, off_t *msrs_pcu_pmon_evtsel, off_t *msrs_pcu_pmon_ctrs)
{
    dump_fixed_counter_data(writedest, msrs_fixed_ctrs, msr_perf_global_ctrl, msr_fixed_counter_ctrl);
    dump_perfmon_counter_data(writedest, msrs_perfevtsel_ctrs, msrs_perfmon_ctrs);
    dump_unc_counter_data(writedest, msrs_pcu_pmon_evtsel, msrs_pcu_pmon_ctrs);
}

void print_all_counter_data(FILE *writedest, off_t *msrs_fixed_ctrs, off_t msr_perf_global_ctrl, off_t msr_fixed_counter_ctrl, off_t *msrs_perfevtsel_ctrs, off_t *msrs_perfmon_ctrs, off_t *msrs_pcu_pmon_evtsel, off_t *msrs_pcu_pmon_ctrs)
{
    print_fixed_counter_data(writedest, msrs_fixed_ctrs, msr_perf_global_ctrl, msr_fixed_counter_ctrl);
    print_perfmon_counter_data(writedest, msrs_perfevtsel_ctrs, msrs_perfmon_ctrs);
    print_unc_counter_data(writedest, msrs_pcu_pmon_evtsel, msrs_pcu_pmon_ctrs);
}

void dump_fixed_counter_data(FILE *writedest, off_t *msrs_fixed_ctrs, off_t msr_perf_global_ctrl, off_t msr_fixed_counter_ctrl)
{
    static int init = 0;
    struct fixed_counter *c0, *c1, *c2;
    int i;
    char hostname[1024];
    int nthreads = 0;

    if (!init)
    {
        fprintf(writedest, "_FIXED_COUNTERS | Host | Thread | InstRet | UnhaltClkCycles | UnhaltRefCycles\n");
        init = 1;
    }
    variorum_set_topology(NULL, NULL, &nthreads);
    gethostname(hostname, 1024);
    fixed_counter_storage(&c0, &c1, &c2, msrs_fixed_ctrs);

    read_batch(FIXED_COUNTERS_DATA);
    for (i = 0; i < nthreads; i++)
    {
        fprintf(writedest, "_FIXED_COUNTERS | %s | %d | %lu | %lu | %lu\n", hostname, i, *c0->value[i], *c1->value[i], *c2->value[i]);
    }
}

void dump_perfmon_counter_data(FILE *writedest, off_t *msrs_perfevtsel_ctrs, off_t *msrs_perfmon_ctrs)
{
    static struct pmc *p = NULL;
    static int init = 0;
    int i;
    char hostname[1024];
    int nthreads;
    int avail = 0;

    gethostname(hostname, 1024);
    avail = cpuid_num_pmc();
    variorum_set_topology(NULL, NULL, &nthreads);

    if (p == NULL && !init)
    {
        switch(avail)
        {
            case 8:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | Host | Thread | PMC0 | PMC1 | PMC2 | PMC3 | PMC4 | PMC5 | PMC6 | PMC7\n");
                break;
            case 7:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | Host | Thread | PMC0 | PMC1 | PMC2 | PMC3 | PMC4 | PMC5 | PMC6\n");
                break;
            case 6:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | Host | Thread | PMC0 | PMC1 | PMC2 | PMC3 | PMC4 | PMC5\n");
                break;
            case 5:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | Host | Thread | PMC0 | PMC1 | PMC2 | PMC3 | PMC4\n");
                break;
            case 4:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | Host | Thread | PMC0 | PMC1 | PMC2 | PMC3\n");
                break;
            case 3:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | Host | Thread | PMC0 | PMC1 | PMC2\n");
                break;
            case 2:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | Host | Thread | PMC0 | PMC1\n");
                break;
            case 1:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | Host | Thread | PMC0\n");
                break;
        }

        set_all_pmc_ctrl(0x0, 0x67, 0x00, 0xC4, 1, msrs_perfevtsel_ctrs);
        set_all_pmc_ctrl(0x0, 0x67, 0x00, 0xC4, 2, msrs_perfevtsel_ctrs);
        enable_pmc(msrs_perfevtsel_ctrs, msrs_perfmon_ctrs);
        pmc_storage(&p, msrs_perfmon_ctrs);
        init = 1;
    }

    read_batch(COUNTERS_DATA);
    for (i = 0; i < nthreads; i++)
    {
        switch (avail)
        {
            case 8:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | %s | %d | %lu | %lu | %lu | %lu | %lu | %lu | %lu | %lu\n",
                        hostname, i, *p->pmc0[i], *p->pmc1[i], *p->pmc2[i], *p->pmc3[i], *p->pmc4[i], *p->pmc5[i], *p->pmc6[i], *p->pmc7[i]);
                break;
            case 7:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | %s | %d | %lu | %lu | %lu | %lu | %lu | %lu | %lu\n",
                        hostname, i, *p->pmc0[i], *p->pmc1[i], *p->pmc2[i], *p->pmc3[i], *p->pmc4[i], *p->pmc5[i], *p->pmc6[i]);
                break;
            case 6:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | %s | %d | %lu | %lu | %lu | %lu | %lu | %lu\n",
                        hostname, i, *p->pmc0[i], *p->pmc1[i], *p->pmc2[i], *p->pmc3[i], *p->pmc4[i], *p->pmc5[i]);
                break;
            case 5:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | %s | %d | %lu | %lu | %lu | %lu | %lu\n",
                        hostname, i, *p->pmc0[i], *p->pmc1[i], *p->pmc2[i], *p->pmc3[i], *p->pmc4[i]);
                break;
            case 4:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | %s | %d | %lu | %lu | %lu | %lu\n",
                        hostname, i, *p->pmc0[i], *p->pmc1[i], *p->pmc2[i], *p->pmc3[i]);
                break;
            case 3:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | %s | %d | %lu | %lu | %lu\n",
                        hostname, i, *p->pmc0[i], *p->pmc1[i], *p->pmc2[i]);
                break;
            case 2:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | %s | %d | %lu | %lu\n",
                        hostname, i, *p->pmc0[i], *p->pmc1[i]);
                break;
            case 1:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS | %s | %d | %lu\n",
                        hostname, i, *p->pmc0[i]);
                break;
        }
    }
}

void print_fixed_counter_data(FILE *writedest, off_t *msrs_fixed_ctrs, off_t msr_perf_global_ctrl, off_t msr_fixed_counter_ctrl)
{
    static int init = 0;
    struct fixed_counter *c0, *c1, *c2;
    int i;
    char hostname[1024];
    int nthreads = 0;

    if (!init)
    {
        init = 1;
    }
    variorum_set_topology(NULL, NULL, &nthreads);
    gethostname(hostname, 1024);
    fixed_counter_storage(&c0, &c1, &c2, msrs_fixed_ctrs);

    read_batch(FIXED_COUNTERS_DATA);
    for (i = 0; i < nthreads; i++)
    {
        fprintf(writedest, "_FIXED_COUNTERS Host: %s Thread: %d InstRet: %lu UnhaltClkCycles: %lu UnhaltRefCycles: %lu\n",
                hostname, i, *c0->value[i], *c1->value[i], *c2->value[i]);
    }
}

void print_perfmon_counter_data(FILE *writedest, off_t *msrs_perfevtsel_ctrs, off_t *msrs_perfmon_ctrs)
{
    static struct pmc *p = NULL;
    static int init = 0;
    int i;
    char hostname[1024];
    int nthreads;
    int avail = 0;

    gethostname(hostname, 1024);
    avail = cpuid_num_pmc();
    variorum_set_topology(NULL, NULL, &nthreads);

    if (p == NULL && !init)
    {
        set_all_pmc_ctrl(0x0, 0x67, 0x00, 0xC4, 1, msrs_perfevtsel_ctrs);
        set_all_pmc_ctrl(0x0, 0x67, 0x00, 0xC4, 2, msrs_perfevtsel_ctrs);
        enable_pmc(msrs_perfevtsel_ctrs, msrs_perfmon_ctrs);
        pmc_storage(&p, msrs_perfmon_ctrs);
        init = 1;
    }

    read_batch(COUNTERS_DATA);
    for (i = 0; i < nthreads; i++)
    {
        switch (avail)
        {
            case 8:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS Host: %s Thread: %d PMC0: %lu PMC1: %lu PMC2: %lu PMC3: %lu PMC4: %lu PMC5: %lu PMC6: %lu PMC7: %lu\n",
                        hostname, i, *p->pmc0[i], *p->pmc1[i], *p->pmc2[i], *p->pmc3[i], *p->pmc4[i], *p->pmc5[i], *p->pmc6[i], *p->pmc7[i]);
                break;
            case 7:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS Host: %s Thread: %d PMC0: %lu PMC1: %lu PMC2: %lu PMC3: %lu PMC4: %lu PMC5: %lu PMC6: %lu\n",
                        hostname, i, *p->pmc0[i], *p->pmc1[i], *p->pmc2[i], *p->pmc3[i], *p->pmc4[i], *p->pmc5[i], *p->pmc6[i]);
                break;
            case 6:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS Host: %s Thread: %d PMC0: %lu PMC1: %lu PMC2: %lu PMC3: %lu PMC4: %lu PMC5: %lu\n",
                        hostname, i, *p->pmc0[i], *p->pmc1[i], *p->pmc2[i], *p->pmc3[i], *p->pmc4[i], *p->pmc5[i]);
                break;
            case 5:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS Host: %s Thread: %d PMC0: %lu PMC1: %lu PMC2: %lu PMC3: %lu PMC4: %lu\n",
                        hostname, i, *p->pmc0[i], *p->pmc1[i], *p->pmc2[i], *p->pmc3[i], *p->pmc4[i]);
                break;
            case 4:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS Host: %s Thread: %d PMC0: %lu PMC1: %lu PMC2: %lu PMC3: %lu\n",
                        hostname, i, *p->pmc0[i], *p->pmc1[i], *p->pmc2[i], *p->pmc3[i]);
                break;
            case 3:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS Host: %s Thread: %d PMC0: %lu PMC1: %lu PMC2: %lu\n",
                        hostname, i, *p->pmc0[i], *p->pmc1[i], *p->pmc2[i]);
                break;
            case 2:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS Host: %s Thread: %d PMC0: %lu PMC1: %lu\n",
                        hostname, i, *p->pmc0[i], *p->pmc1[i]);
                break;
            case 1:
                fprintf(writedest, "_PERFORMANCE_MONITORING_COUNTERS Host: %s Thread: %d PMC0: %lu\n",
                        hostname, i, *p->pmc0[i]);
                break;
        }
    }
}

/**************************************/
/* Programmable Performance Counters  */
/**************************************/

/// @brief Initialize storage for general-purpose performance counter data.
///
/// @param [out] p Data for general-purpose performance counters.
/// @param [in] msrs_perfmon_ctrs Array of unique addresses for PERFMON_CTRS.
///
/// @return 0 if successful, else -1 if number of general-purpose performance
/// counters is less than 1.
static int init_pmc(struct pmc *p, off_t *msrs_perfmon_ctrs)
{
    int nthreads = 0;
    int avail = cpuid_num_pmc();

    variorum_set_topology(NULL, NULL, &nthreads);

    if (avail < 1)
    {
        return -1;
    }
    switch (avail)
    {
        case 8:
            p->pmc7 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
        case 7:
            p->pmc6 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
        case 6:
            p->pmc5 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
        case 5:
            p->pmc4 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
        case 4:
            p->pmc3 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
        case 3:
            p->pmc2 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
        case 2:
            p->pmc1 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
        case 1:
            p->pmc0 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
    }
    allocate_batch(COUNTERS_DATA, avail * nthreads);
    switch (avail)
    {
        case 8:
            load_thread_batch(msrs_perfmon_ctrs[7], p->pmc7, COUNTERS_DATA);
        case 7:
            load_thread_batch(msrs_perfmon_ctrs[6], p->pmc6, COUNTERS_DATA);
        case 6:
            load_thread_batch(msrs_perfmon_ctrs[5], p->pmc5, COUNTERS_DATA);
        case 5:
            load_thread_batch(msrs_perfmon_ctrs[4], p->pmc4, COUNTERS_DATA);
        case 4:
            load_thread_batch(msrs_perfmon_ctrs[3], p->pmc3, COUNTERS_DATA);
        case 3:
            load_thread_batch(msrs_perfmon_ctrs[2], p->pmc2, COUNTERS_DATA);
        case 2:
            load_thread_batch(msrs_perfmon_ctrs[1], p->pmc1, COUNTERS_DATA);
        case 1:
            load_thread_batch(msrs_perfmon_ctrs[0], p->pmc0, COUNTERS_DATA);
    }
    return 0;
}

/// @brief Initialize storage for performance event select counter data.
///
/// @param [out] evt Data for performance event select counters.
/// @param [in] msrs_perfevtsel_ctrs Array of unique addresses for
///        PERFEVTSEL_CTRS.
///
/// @return 0 if successful, else -1 if number of general-purpose performance
/// counters is less than 1.
static int init_perfevtsel(struct perfevtsel *evt, off_t *msrs_perfevtsel_ctrs)
{
    int nthreads;
    int avail = cpuid_num_pmc();

    variorum_set_topology(NULL, NULL, &nthreads);

    if (avail < 1)
    {
        return -1;
    }
    switch (avail)
    {
        case 8:
            evt->perf_evtsel7 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
        case 7:
            evt->perf_evtsel6 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
        case 6:
            evt->perf_evtsel5 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
        case 5:
            evt->perf_evtsel4 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
        case 4:
            evt->perf_evtsel3 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
        case 3:
            evt->perf_evtsel2 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
        case 2:
            evt->perf_evtsel1 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
        case 1:
            evt->perf_evtsel0 = (uint64_t **) calloc(nthreads, sizeof(uint64_t *));
    }
    allocate_batch(COUNTERS_CTRL, avail * nthreads);
    switch (avail)
    {
        case 8:
            load_thread_batch(msrs_perfevtsel_ctrs[7], evt->perf_evtsel7, COUNTERS_CTRL);
        case 7:
            load_thread_batch(msrs_perfevtsel_ctrs[6], evt->perf_evtsel6, COUNTERS_CTRL);
        case 6:
            load_thread_batch(msrs_perfevtsel_ctrs[5], evt->perf_evtsel5, COUNTERS_CTRL);
        case 5:
            load_thread_batch(msrs_perfevtsel_ctrs[4], evt->perf_evtsel4, COUNTERS_CTRL);
        case 4:
            load_thread_batch(msrs_perfevtsel_ctrs[3], evt->perf_evtsel3, COUNTERS_CTRL);
        case 3:
            load_thread_batch(msrs_perfevtsel_ctrs[2], evt->perf_evtsel2, COUNTERS_CTRL);
        case 2:
            load_thread_batch(msrs_perfevtsel_ctrs[1], evt->perf_evtsel1, COUNTERS_CTRL);
        case 1:
            load_thread_batch(msrs_perfevtsel_ctrs[0], evt->perf_evtsel0, COUNTERS_CTRL);
    }
    return 0;
}

void set_all_pmc_ctrl(uint64_t cmask, uint64_t flags, uint64_t umask, uint64_t eventsel, int pmcnum, off_t *msrs_perfevtsel_ctrs)
{
    int nthreads;
    int i;

    variorum_set_topology(NULL, NULL, &nthreads);
    for (i = 0; i < nthreads; i++)
    {
        set_pmc_ctrl_flags(cmask, flags, umask, eventsel, pmcnum, i, msrs_perfevtsel_ctrs);
    }
}

int enable_pmc(off_t *msrs_perfevtsel_ctrs, off_t *msrs_perfmon_ctrs)
{
    static struct perfevtsel *evt = NULL;
    static int avail = 0;

    if (evt == NULL)
    {
        avail = cpuid_num_pmc();
        if (avail == 0)
        {
            return -1;
        }
        perfevtsel_storage(&evt, msrs_perfevtsel_ctrs);
    }
    write_batch(COUNTERS_CTRL);
    clear_all_pmc(msrs_perfmon_ctrs);
    return 0;
}

/* IA32_PEREVTSELx MSRs
 * cmask [31:24]
 * flags [23:16]
 * umask [15:8]
 * eventsel [7:0]
 */
void set_pmc_ctrl_flags(uint64_t cmask, uint64_t flags, uint64_t umask, uint64_t eventsel, int pmcnum, unsigned thread, off_t *msrs_perfevtsel_ctrs)
{
    static struct perfevtsel *evt = NULL;
    if (evt == NULL)
    {
        perfevtsel_storage(&evt, msrs_perfevtsel_ctrs);
    }
    switch(pmcnum)
    {
        case 8:
            *evt->perf_evtsel7[thread] = 0UL | (cmask << 24) | (flags << 16) | (umask << 8) | eventsel;
            break;
        case 7:
            *evt->perf_evtsel6[thread] = 0UL | (cmask << 24) | (flags << 16) | (umask << 8) | eventsel;
            break;
        case 6:
            *evt->perf_evtsel5[thread] = 0UL | (cmask << 24) | (flags << 16) | (umask << 8) | eventsel;
            break;
        case 5:
            *evt->perf_evtsel4[thread] = 0UL | (cmask << 24) | (flags << 16) | (umask << 8) | eventsel;
            break;
        case 4:
            *evt->perf_evtsel3[thread] = 0UL | (cmask << 24) | (flags << 16) | (umask << 8) | eventsel;
            break;
        case 3:
            *evt->perf_evtsel2[thread] = 0UL | (cmask << 24) | (flags << 16) | (umask << 8) | eventsel;
            break;
        case 2:
            *evt->perf_evtsel1[thread] = 0UL | (cmask << 24) | (flags << 16) | (umask << 8) | eventsel;
            break;
        case 1:
            *evt->perf_evtsel0[thread] = 0UL | (cmask << 24) | (flags << 16) | (umask << 8) | eventsel;
            break;
    }
}

void perfevtsel_storage(struct perfevtsel **e, off_t *msrs_perfevtsel_ctrs)
{
    static struct perfevtsel evt;
    static int init = 0;

    if (!init)
    {
        init_perfevtsel(&evt, msrs_perfevtsel_ctrs);
        init = 1;
    }
    if (e != NULL)
    {
        *e = &evt;
    }
}

void pmc_storage(struct pmc **p, off_t *msrs_perfmon_ctrs)
{
    static struct pmc counters;
    static int init = 0;

    if (!init)
    {
        init_pmc(&counters, msrs_perfmon_ctrs);
        init = 1;
    }
    if (p != NULL)
    {
        *p = &counters;
    }
}

void clear_all_pmc(off_t *msrs_perfmon_ctrs)
{
    static struct pmc *p = NULL;
    static int nthreads = 0;
    static int avail = 0;
    int i;

    if (p == NULL)
    {
        avail = cpuid_num_pmc();
        variorum_set_topology(NULL, NULL, &nthreads);
        pmc_storage(&p, msrs_perfmon_ctrs);
    }
    for (i = 0; i < nthreads; i++)
    {
        switch (avail)
        {
            case 8:
                *p->pmc7[i] = 0;
            case 7:
                *p->pmc6[i] = 0;
            case 6:
                *p->pmc5[i] = 0;
            case 5:
                *p->pmc4[i] = 0;
            case 4:
                *p->pmc3[i] = 0;
            case 3:
                *p->pmc2[i] = 0;
            case 2:
                *p->pmc1[i] = 0;
            case 1:
                *p->pmc0[i] = 0;
        }
    }
    write_batch(COUNTERS_DATA);
}

///*************************************/
///* Uncore PCU Performance Monitoring */
///*************************************/

/// @brief Initialize storage for uncore performance event select counter data.
///
/// @param [out] uevt Data for uncore performance event select counters.
/// @param [in] msrs_pcu_pmon_evtsel Array of unique addresses for
///        MSR_PCU_PMON_EVTSEL.
static void init_unc_perfevtsel(struct unc_perfevtsel *uevt, off_t *msrs_pcu_pmon_evtsel)
{
    static int init = 0;
    int nsockets;

    variorum_set_topology(&nsockets, NULL, NULL);

    if (!init)
    {
        uevt->c0 = (uint64_t **) calloc(nsockets, sizeof(uint64_t *));
        uevt->c1 = (uint64_t **) calloc(nsockets, sizeof(uint64_t *));
        uevt->c2 = (uint64_t **) calloc(nsockets, sizeof(uint64_t *));
        uevt->c3 = (uint64_t **) calloc(nsockets, sizeof(uint64_t *));
        allocate_batch(UNCORE_EVTSEL, 4 * nsockets);
        load_socket_batch(msrs_pcu_pmon_evtsel[0], uevt->c0, UNCORE_EVTSEL);
        load_socket_batch(msrs_pcu_pmon_evtsel[1], uevt->c1, UNCORE_EVTSEL);
        load_socket_batch(msrs_pcu_pmon_evtsel[2], uevt->c2, UNCORE_EVTSEL);
        load_socket_batch(msrs_pcu_pmon_evtsel[3], uevt->c3, UNCORE_EVTSEL);
        init = 1;
    }
}

/// @brief Initialize storage for uncore general-purpose performance event
/// counter data.
///
/// @param [out] uc Data for uncore general-purpose performance counters.
/// @param [in] msrs_pcu_pmon_ctrs Array of unique addresses for
///        MSR_PCU_PMON_CTRS.
static void init_unc_counters(struct unc_counters *uc, off_t *msrs_pcu_pmon_ctrs)
{
    static int init = 0;
    int nsockets;

    variorum_set_topology(&nsockets, NULL, NULL);
    if (!init)
    {
        uc->c0 = (uint64_t **) calloc(nsockets, sizeof(uint64_t *));
        uc->c1 = (uint64_t **) calloc(nsockets, sizeof(uint64_t *));
        uc->c2 = (uint64_t **) calloc(nsockets, sizeof(uint64_t *));
        uc->c3 = (uint64_t **) calloc(nsockets, sizeof(uint64_t *));
        allocate_batch(UNCORE_COUNT, 4 * nsockets);
        load_socket_batch(msrs_pcu_pmon_ctrs[0], uc->c0, UNCORE_COUNT);
        load_socket_batch(msrs_pcu_pmon_ctrs[1], uc->c1, UNCORE_COUNT);
        load_socket_batch(msrs_pcu_pmon_ctrs[2], uc->c2, UNCORE_COUNT);
        load_socket_batch(msrs_pcu_pmon_ctrs[3], uc->c3, UNCORE_COUNT);
        init = 1;
    }
}

void unc_perfevtsel_storage(struct unc_perfevtsel **uevt, off_t *msrs_pcu_pmon_evtsel)
{
    static struct unc_perfevtsel uevt_data;
    static int init = 0;
    if (!init)
    {
        init = 1;
        init_unc_perfevtsel(&uevt_data, msrs_pcu_pmon_evtsel);
    }
    if (uevt != NULL)
    {
        *uevt = &uevt_data;
    }
}

void unc_counters_storage(struct unc_counters **uc, off_t *msrs_pcu_pmon_ctrs)
{
    static struct unc_counters uc_data;
    static int init = 0;
    if (!init)
    {
        init = 1;
        init_unc_counters(&uc_data, msrs_pcu_pmon_ctrs);
    }
    if (uc != NULL)
    {
        *uc = &uc_data;
    }
}

void enable_pcu(off_t *msrs_pcu_pmon_evtsel, off_t *msrs_pcu_pmon_ctrs)
{
    static struct unc_perfevtsel *uevt = NULL;
    if (uevt == NULL)
    {
        unc_perfevtsel_storage(&uevt, msrs_pcu_pmon_evtsel);
    }
    write_batch(UNCORE_EVTSEL);
    clear_all_pcu(msrs_pcu_pmon_ctrs);
}

void clear_all_pcu(off_t *msrs_pcu_pmon_ctrs)
{
    static struct unc_counters *uc = NULL;
    int nsockets = 0;
    int i;

    variorum_set_topology(&nsockets, NULL, NULL);
    if (uc == NULL)
    {
        unc_counters_storage(&uc, msrs_pcu_pmon_ctrs);
    }
    for (i = 0; i < nsockets; i++)
    {
        *uc->c0[i] = 0;
        *uc->c1[i] = 0;
        *uc->c2[i] = 0;
        *uc->c3[i] = 0;
    }
    write_batch(UNCORE_COUNT);
}

void dump_unc_counter_data(FILE *writedest, off_t *msrs_pcu_pmon_evtsel, off_t *msrs_pcu_pmon_ctrs)
{
    static int init = 0;
    struct unc_counters *uc;
    int i;
    int nsockets;
    char hostname[1024];

    variorum_set_topology(&nsockets, NULL, NULL);
    gethostname(hostname, 1024);

    unc_counters_storage(&uc, msrs_pcu_pmon_ctrs);
    if (!init)
    {
        fprintf(writedest, "_UNCORE_COUNTERS | Host | Socket | c0 | c1 | c2 | c3\n");
        enable_pcu(msrs_pcu_pmon_evtsel, msrs_pcu_pmon_ctrs);
        init = 1;
    }
    for (i = 0; i < nsockets; i++)
    {
        fprintf(writedest, "_UNCORE_COUNTERS | %s | %d | %lx | %lx | %lx | %lx\n", hostname, i, *uc->c0[i], *uc->c1[i], *uc->c2[i], *uc->c3[i]);
    }
}

void print_unc_counter_data(FILE *writedest, off_t *msrs_pcu_pmon_evtsel, off_t *msrs_pcu_pmon_ctrs)
{
    struct unc_counters *uc;
    int i;
    int nsockets;
    char hostname[1024];

    variorum_set_topology(&nsockets, NULL, NULL);
    gethostname(hostname, 1024);

    unc_counters_storage(&uc, msrs_pcu_pmon_ctrs);
    enable_pcu(msrs_pcu_pmon_evtsel, msrs_pcu_pmon_ctrs);
    for (i = 0; i < nsockets; i++)
    {
        fprintf(writedest, "_UNCORE_COUNTERS Host: %s Socket: %d c0: %lx c1: %lx c2: %lx c3: %lx\n",
                hostname, i, *uc->c0[i], *uc->c1[i], *uc->c2[i], *uc->c3[i]);
    }
}

void init_aperf(struct clocks_data **cd, int nthreads)
{
    const off_t ia32_aperf = 0xE8;
    static struct clocks_data d;
  
    d.aperf = (uint64_t **) malloc(nthreads * sizeof(uint64_t *)); 
    allocate_batch(USR_BATCH0, 1UL * nthreads);
    load_thread_batch(ia32_aperf, d.aperf, USR_BATCH0);
    if (cd != NULL)
    {
        *cd = &d;
    }
}

int aperf_monitor(FILE *outfile)
{
    const off_t ia32_aperf = 0xE8;
    static struct clocks_data *cd; 
    static struct timespec t1;
    struct timespec t2;
    uint64_t elapsed;
    int idx, i, j, k;
    static int init = 0;
    static int nsockets, ncores, nthreads;
    const int BUFF_SIZE= 1024;
    char file_buffer[BUFF_SIZE + 64]; 
    static int buffercount;
  
    if(!init)
    {
        variorum_set_topology(&nsockets, &ncores, &nthreads);
        init_aperf(&cd, nthreads);
        buffercount = 0;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        init = 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);
    elapsed = BILLION * (t2.tv_sec - t1.tv_sec) + t2.tv_nsec - t1.tv_nsec; //nanoseconds

    read_batch(USR_BATCH0);
    for(i=0; i < nsockets; i++)
    {
        for(j=0; j<ncores/nsockets; j++)
        {
            for(k=0; k < nthreads/ncores; k++)
            {
                idx = (k*nsockets*(ncores/nsockets)) + (i*(ncores/nsockets)) + j;
                buffercount=buffercount+sprintf(&file_buffer[buffercount], "%d, %lu, %d\n", j,*cd->aperf[idx], elapsed);
            }
        }
        fwrite(file_buffer, buffercount, 1, outfile);
        buffercount = 0;
    }
    fwrite(file_buffer, buffercount, 1, outfile);
    buffercount = 0;

    return 0;
}

void mon_storage(struct perf_data **pd, struct clocks_data **cd, off_t msr_aperf, off_t msr_mperf, off_t msr_tsc, struct fixed_counter **ctr0, struct fixed_counter **ctr1, struct fixed_counter **ctr2, off_t *msrs_fixed_ctrs, off_t msr_perf_status)
{
    static int init = 0;
    static struct clocks_data d;
    static struct perf_data p;
    static struct fixed_counter c0, c1, c2;
    static int nthreads = 0;
    static int nsockets = 0;

    if (!init)
    {
        variorum_set_topology(&nsockets, NULL, &nthreads);
        init_fixed_counter(&c0);
        init_fixed_counter(&c1);
        init_fixed_counter(&c2);
        d.aperf = (uint64_t **) malloc(nthreads * sizeof(uint64_t *));
        d.mperf = (uint64_t **) malloc(nthreads * sizeof(uint64_t *));
        d.tsc = (uint64_t **) malloc(nthreads * sizeof(uint64_t *));
        p.perf_status = (uint64_t **) malloc(nsockets * sizeof(uint64_t *));
        allocate_batch(USR_BATCH0, 7UL * nthreads);
        load_thread_batch(msr_aperf, d.aperf, USR_BATCH0);
        load_thread_batch(msr_mperf, d.mperf, USR_BATCH0);
        load_thread_batch(msr_tsc, d.tsc, USR_BATCH0);
        load_socket_batch(msr_perf_status, p.perf_status, USR_BATCH0);
        load_thread_batch(msrs_fixed_ctrs[0], c0.value, USR_BATCH0);
        load_thread_batch(msrs_fixed_ctrs[1], c1.value, USR_BATCH0);
        load_thread_batch(msrs_fixed_ctrs[2], c2.value, USR_BATCH0);
        init = 1;
    }

    if (cd != NULL)
    {
        *cd = &d;
    }

    if (pd != NULL)
    {
        *pd = &p;
    }

    if (ctr0 != NULL)
    {
        *ctr0 = &c0;
    }
    if (ctr1 != NULL)
    {
        *ctr1 = &c1;
    }
    if (ctr2 != NULL)
    {
        *ctr2 = &c2;
    }
}

void read_fixed_counter_data(FILE *writedest, off_t *msrs_fixed_ctrs, off_t msr_perf_global_ctrl, off_t msr_fixed_counter_ctrl, off_t msr_power_limit, off_t msr_rapl_unit, off_t msr_pkg_energy_status, off_t msr_dram_energy_status)
{
    struct fixed_counter *c0, *c1, *c2;
    char hostname[1024];
    int idx;
    int i, j, k;
    int nsockets, ncores, nthreads;
    static int init = 0;
    static struct rapl_data *rapl = NULL;
    static struct timeval start;
    struct timeval now;

    variorum_set_topology(&nsockets, &ncores, &nthreads);

    gethostname(hostname, 1024);
    fixed_counter_storage(&c0, &c1, &c2, msrs_fixed_ctrs);

    get_power(msr_rapl_unit, msr_pkg_energy_status, msr_dram_energy_status);

    if (!init)
    {
        gettimeofday(&start, NULL);
        rapl_storage(&rapl);
    }
    gettimeofday(&now, NULL);

    read_batch(FIXED_COUNTERS_DATA);
    for (i = 0; i < nsockets; i++)
    {
        for (j = 0; j < ncores/nsockets; j++)
        {
            for (k = 0; k < nthreads/ncores; k++)
            {
                idx = (k * nsockets * (ncores/nsockets)) + (i * (ncores/nsockets)) + j;
                fprintf(writedest, "%s, %d, %d, %d, %lu, %lu, %lu, %lf, %lf \n", hostname, i, j, idx, *c0->value[idx], *c1->value[idx], *c2->value[idx],rapl->pkg_joules[i], rapl->pkg_watts[i]);
            }
        }
    }
}

void get_monitoring_data(FILE *writedest, off_t *msrs_fixed_ctrs, off_t msr_perf_global_ctrl, off_t msr_fixed_counter_ctrl, off_t msr_power_limit, off_t msr_rapl_unit, off_t msr_pkg_energy_status, off_t msr_dram_energy_status, off_t msr_aperf, off_t msr_mperf, off_t msr_tsc, off_t msr_perf_status)
{
    int idx, i, j, k;
    static int nsockets, ncores, nthreads;
    static int init = 0;
    char file_buffer[CHUNK_SIZE + 1024];
    static int buffer_count = 0;

    static struct timeval start;
    struct timeval now;
    int prev_aperf = 0;
    int prev_mperf = 0;
    int prev_c0 = 0;
    int prev_c1 = 0;
    int aperf_delta, mperf_delta, c0_delta, c1_delta;
    static struct timespec t1;
    struct timespec t2;
    uint64_t elapsed;

    struct fixed_counter *c0, *c1, *c2;
    static struct rapl_data *rapl = NULL;
    //struct pkg_therm_stat *pkg_stat = NULL;
    //struct therm_stat *t_stat = NULL;

    //get_power(msr_rapl_unit, msr_pkg_energy_status, msr_dram_energy_status);


    //t_stat = (struct therm_stat *) malloc(nthreads * sizeof(struct therm_stat));
    //get_therm_stat(t_stat, msr_therm_stat);

    static struct clocks_data *cd;
    static struct perf_data *pd;

    if (!init)
    {
       variorum_set_topology(&nsockets, &ncores, &nthreads);
       buffer_count = 0;
       //gettimeofday(&start, NULL);
       mon_storage(&pd, &cd, msr_aperf, msr_mperf, msr_tsc, &c0, &c1, &c2, msrs_fixed_ctrs, msr_perf_status);
       enable_fixed_counters(msrs_fixed_ctrs, msr_perf_global_ctrl, msr_fixed_counter_ctrl);
       rapl_storage(&rapl);
       clock_gettime(CLOCK_MONOTONIC, &t1);
       init = 1;
    }
    //pkg_stat = (struct pkg_therm_stat *) malloc(nsockets * sizeof(struct pkg_therm_stat));
    //get_pkg_therm_stat(pkg_stat, msr_pkg_therm_stat);

    clock_gettime(CLOCK_MONOTONIC, &t2);
    elapsed = BILLION * (t2.tv_sec - t1.tv_sec) + t2.tv_nsec - t1.tv_nsec; //nanoseconds
    //get_power(msr_rapl_unit, msr_pkg_energy_status, msr_dram_energy_status);
    //gettimeofday(&now, NULL);

    read_batch(USR_BATCH0);
    for (i = 0; i < nsockets; i++)
    {
        for (j = 0; j < ncores/nsockets; j++)
        {
            for (k = 0; k < nthreads/ncores; k++)
            {
                idx = (k * nsockets * (ncores/nsockets)) + (i * (ncores/nsockets)) + j;
                aperf_delta = *cd->aperf[idx] - prev_aperf;
                mperf_delta = *cd->mperf[idx] - prev_mperf;
                c0_delta = *c0->value[idx] - prev_c0;
                c1_delta = *c1->value[idx] - prev_c1;
                if (idx != 0)
                {
                    prev_aperf = *cd->aperf[idx-1];
                    prev_mperf = *cd->mperf[idx-1];
                    prev_c0 = *c0->value[idx-1];
                    prev_c1 = *c1->value[idx-1];
                }
                else
                {
                    prev_aperf = *cd->aperf[idx];
                    prev_mperf = *cd->mperf[idx];
                    prev_c0 = *c0->value[idx];
                    prev_c1 = *c1->value[idx];
                }

                buffer_count = buffer_count + sprintf(&file_buffer[buffer_count], "%d,%d,%d,%lu,%lu,%lu,%lu,%lu,%lu\n", i, j, idx, (long long unsigned int)(elapsed),*c0->value[idx], *c1->value[idx], aperf_delta, mperf_delta, MASK_VAL(*pd->perf_status[i], 15, 8));

                //buffer_count= buffer_count + sprintf(&file_buffer[buffer_count], "%d,%d,%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lf,%lf,%d\n", i, j, idx, (long long unsigned int)(elapsed),*c0->value[idx], *c1->value[idx], *c2->value[idx], *cd->aperf[idx], *cd->mperf[idx], *cd->tsc[idx], MASK_VAL(*pd->perf_status[i], 15, 8), rapl->pkg_joules[i], rapl->pkg_watts[i], pkg_stat[i].readout);
            }
        }
        fwrite(file_buffer, buffer_count, 1, writedest);
        buffer_count = 0;
    }
    if (buffer_count > 0)
    {
        fwrite(file_buffer, buffer_count, 1, writedest);
        buffer_count = 0;
    }

    //free(pkg_stat);
    //free(t_stat);
}
