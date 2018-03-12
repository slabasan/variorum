/* power_wrapper_dynamic.c
 *
 * Copyright (c) 2011-2016, Lawrence Livermore National Security, LLC.
 * LLNL-CODE-645430
 *
 * Produced at Lawrence Livermore National Laboratory
 * Written by  Barry Rountree,   rountree@llnl.gov
 *             Daniel Ellsworth, ellsworth8@llnl.gov
 *             Scott Walker,     walker91@llnl.gov
 *             Kathleen Shoga,   shoga1@llnl.gov
 *
 * All rights reserved.
 *
 * This file is part of libmsr.
 *
 * libmsr is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * libmsr is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libmsr. If not, see <http://www.gnu.org/licenses/>.
 *
 * This material is based upon work supported by the U.S. Department of
 * Energy's Lawrence Livermore National Laboratory. Office of Science, under
 * Award number DE-AC52-07NA27344.
 *
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "highlander.h"

#if 0
/********/
/* RAPL */
/********/
static double total_joules = 0.0;
static double limit_joules = 0.0;
static double max_watts = 0.0;
static double min_watts = 1024.0;
#endif

static unsigned long start;
static unsigned long end;
static FILE *logfile = NULL;
static FILE *summaryfile = NULL;
static double watt_cap = 0.0;
static volatile int poll_num = 0;
static volatile int poll_dir = 5;

static pthread_mutex_t mlock;
static int *shmseg;
static int shmid;

static int running = 1;

#include "common.c"

void *power_set_measurement(void *arg)
{
    unsigned long poll_num = 0;
    struct mstimer timer;
    //set_rapl_power(watt_cap, watt_cap);
    set_each_socket_power_limit(watt_cap);
    double watts = watt_cap;
    // According to the Intel docs, the counter wraps a most once per second.
    // 100 ms should be short enough to always get good information.
    init_msTimer(&timer, 1500);
    init_data();
    start = now_ms();

    poll_num++;
    timer_sleep(&timer);
    while (running)
    {
        take_measurement();
        if (poll_num%5 == 0)
        {
            if (watts >= watt_cap)
            {
                poll_dir = -5;
            }
            if (watts <= 30)
            {
                poll_dir = 5;
            }
            watts += poll_dir;
            //set_rapl_power(watts, watts);
            set_each_socket_power_limit(watt_cap);
        }
        poll_num++;
        timer_sleep(&timer);
    }
}

int main(int argc, char**argv)
{
    const char *usage = "\n"
                        "NAME\n"
                        "  power_wrapper_dynamic - Package and DRAM power\n"
                        "  monitor, adjusting the power cap stewise every\n"
                        "  500 ms.\n"
                        "SYNOPSIS\n"
                        "  %s [--help | -h] [-c] -w -a \"<executable> <args> ...\"\n"
                        "OVERVIEW\n"
                        "  Power_wrapper_dynamic is a utility for adjusting\n"
                        "  the power cap stepwise every 500 ms, and sampling\n"
                        "  and printing the power consumption (for package\n"
                        "  and DRAM) and power limit per socket for systems\n"
                        "  with two sockets.\n"
                        "OPTIONS\n"
                        "  --help | -h\n"
                        "      Display this help information, then exit.\n"
                        "  -a\n"
                        "      Application and arguments in quotes.\n"
                        "  -w\n"
                        "      Power cap.\n"
                        "  -c\n"
                        "      Remove stale shared memory.\n"
                        "\n";
    if (argc == 1 || (argc > 1 && (
                          strncmp(argv[1], "--help", strlen("--help")) == 0 ||
                          strncmp(argv[1], "-h", strlen("-h")) == 0 )))
    {
        printf(usage, argv[0]);
        return 0;
    }

    int opt;
    int app_flag = 0;
    int watts_flag = 0;
    char *app;
    char **arg = NULL;

    while ((opt = getopt(argc, argv, "cw:a:")) != -1)
    {
        switch(opt)
        {
            case 'c':
                highlander_clean();
                printf("Exiting power_wrapper_dynamic...\n");
                return 0;
            case 'a':
                app_flag = 1;
                app = optarg;
                break;
            case 'w':
                watts_flag = 1;
                watt_cap = atof(optarg);
                break;
            case '?':
                fprintf(stderr, "\nError: unknown parameter \"-%c\"\n", optopt);
                fprintf(stderr, usage, argv[0]);
                return 1;
            default:
                return 1;
        }
    }
    if (app_flag == 0)
    {
        fprintf(stderr, "\nError: must specify \"-a\"\n");
        fprintf(stderr, usage, argv[0]);
        return 1;
    }
    if (watts_flag == 0)
    {
        fprintf(stderr, "\nError: must specify \"-w\"\n");
        fprintf(stderr, usage, argv[0]);
        return 1;
    }

    char *app_split = strtok(app, " ");
    int n_spaces = 0, i;
    while (app_split)
    {
        arg = realloc (arg, sizeof (char*) * ++n_spaces);

        if (arg == NULL)
        {
            return 1; /* memory allocation failed */
        }
        arg[n_spaces-1] = app_split;
        app_split = strtok(NULL, " ");
    }
    arg = realloc (arg, sizeof (char*) * (n_spaces+1));
    arg[n_spaces] = 0;

    if (highlander())
    {
        /* Start the log file. */
        int logfd;
        char hostname[64];
        gethostname(hostname, 64);

        char *fname;
#if 0
        asprintf(&fname, "%s.power.dat", hostname);

        logfd = open(fname, O_WRONLY|O_CREAT|O_EXCL|O_NOATIME|O_NDELAY, S_IRUSR|S_IWUSR);
        if (logfd < 0)
        {
            fprintf(stderr, "Fatal Error: %s on %s cannot open the appropriate fd for %s -- %s.\n", argv[0], hostname, fname, strerror(errno));
            return 1;
        }
        logfile = fdopen(logfd, "w");
        if (logfile == NULL)
        {
            fprintf(stderr, "Fatal Error: %s on %s fdopen failed for %s -- %s.\n", argv[0], hostname, fname, strerror(errno));
            return 1;
        }
#endif

        //read_rapl_init();

        /* Set the cap. */
        //set_rapl_power(watt_cap, watt_cap);
        set_each_socket_power_limit(watt_cap);

        /* Start power measurement thread. */
        pthread_t mthread;
        pthread_create(&mthread, NULL, power_set_measurement, NULL);

        /* Fork. */
        pid_t app_pid = fork();
        if (app_pid == 0)
        {
            /* I'm the child. */
            execvp(arg[0], &arg[0]);
            return 1;
        }
        /* Wait. */
        waitpid(app_pid, NULL, 0);
        sleep(1);

        highlander_wait();

        /* Stop power measurement thread. */
        running = 0;
        take_measurement();
        end = now_ms();

        asprintf(&fname, "%s.power.summary", hostname);

        logfd = open(fname, O_WRONLY|O_CREAT|O_EXCL|O_NOATIME|O_NDELAY, S_IRUSR|S_IWUSR);
        if (logfd < 0)
        {
            fprintf(stderr, "Fatal Error: %s on %s cannot open the appropriate fd for %s -- %s.\n", argv[0], hostname, fname, strerror(errno));
            return 1;
        }
        summaryfile = fdopen(logfd, "w");
        if (summaryfile == NULL)
        {
            fprintf(stderr, "Fatal Error: %s on %s fdopen failed for %s -- %s.\n", argv[0], hostname, fname, strerror(errno));
            return 1;
        }
        /* Output summary data. */
        char *msg;
        //asprintf(&msg, "host: %s\npid: %d\ntotal: %lf\nallocated: %lf\nmax_watts: %lf\nmin_watts: %lf\nruntime ms: %lu\n,start: %lu\nend: %lu\n", hostname, app_pid, total_joules, limit_joules, max_watts, min_watts, end-start, start, end);
        asprintf(&msg, "host: %s\npid: %d\nruntime ms: %lu\nstart: %lu\nend: %lu\n", hostname, app_pid, end-start, start, end);

        fprintf(summaryfile, "%s", msg);
        fclose(summaryfile);
        close(logfd);
        shmctl(shmid, IPC_RMID, NULL);
        shmdt(shmseg);
    }
    else
    {
        /* Fork. */
        pid_t app_pid = fork();
        if (app_pid == 0)
        {
            /* I'm the child. */
            execvp(arg[0], &arg[0]);
            return 1;
        }
        /* Wait. */
        waitpid(app_pid, NULL, 0);

        highlander_wait();
    }

    highlander_clean();
    return 0;
}
