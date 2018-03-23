// Program to monitor performance of a running application and make adjustments
// to RAPL. Runs on a dedicated core
//
// Author: Tiffany A. Connors

#define _GNU_SOURCE
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <variorum.h>

int main(int argc, char **argv)
{
    const char *usage = "\n"
                            "NAME\n"
                            "  pmon - A program for monitoring performance of a running application and to change RAPL settings \n\n"
                            "SYNOPSIS\n"
                            "  %s [--help | -h] [-r ntrials] [-p freq] [-w time] [-l lim] [-b en] -i ms -t sec\n\n"
                            "OVERVIEW\n"
                            "  pmon collects performance data of a running application and is meant to be executed on a decicated core.\n"
                            "  In addition to monitoring, pmon allows provides an interface for adjusting the RAPL lower time window and power limit, 
                            "  enabling or disabling turbo, and requesting a new initial p-state \n\n"
                            "OPTIONS\n"
                            "  -h | --help\n"
                            "      Display this message, then exit.\n"
                            "  -i | --interval\n"
                            "      Required. How often to sample, in microseconds. Pass 0 to sample continuously.\n"
                            "  -t | --time\n"
                            "      Required. The length of time to do sampling, in seconds.\n"
                            "  -r | --runNum\n"
                            "      Optional. If doing multiple experimental runs, this is the current iteration number that specifies which run it is on\n"
                            "  -p | --pstate\n"
                            "      Optional. Request a new p-state ratio i.e. to request a pstate of 2100 MHz, the value 21 would be passed as the p-state ratio\n"
                            "  -w | --timeWindow\n"
                            "      Optional. Change RAPL time window.\n"
                            "  -l | --powLim\n"
                            "      Optional. Change RAPL power limit to the given watts.\n"
                            "  -b | --turbo\n"
                            "      Optional. 0 to disable and 1 to enable.\n"
                            "\n";

    int interval, sampleLength, c;
    int run = 0, pstate = 0;
    double timewindow = 0;
    int continuous = 0;
    int tflag = 0, iflag = 0;
    int turbo = -1;
    int powerLim = 0;

    static struct option long_options[] =
    {
        {"help",       no_argument,       0, 'h'},
        {"interval",   required_argument, 0, 'i'},
        {"time",       required_argument, 0, 't'},
        {"runNum",     required_argument, 0, 'r'},
        {"pstate",     required_argument, 0, 'p'},
        {"timeWindow", required_argument, 0, 'w'},
        {"turbo",      required_argument, 0, 'b'},
        {"powerLim",   required_argument, 0, 'l'},
        {0,            0,                 0,  0 }
    };

    if (argc > 1 && (
            strncmp(argv[1], "--help", strlen("--help")) == 0 ||
            strncmp(argv[1], "-h", strlen("-h")) == 0))
    {
        printf(usage, argv[0]);
        return 0;
    }

    while ((c = getopt_long(argc, argv, "hi:t:r:p:w:b:l:", long_options, NULL)) != -1)
    {
        switch(c)
        {
            case 'h':
                printf(usage, argv[0]);
                exit(0);
                break;
            case 'p':
                pstate = (int)strtol(optarg, NULL, 10);
                break;
            case 'w':
                timewindow=(double)strtol(optarg, NULL, 10);
                break;
            case 'l':
                powerLim = (int)strtol(optarg, NULL, 10);
                break;
            case 'i':
                interval = (int)strtol(optarg, NULL, 10);
                if (interval == 0)
                {
                    continuous = 1;
                }
                iflag = 1;
                break;
            case 't':
                sampleLength = (int)strtol(optarg, NULL, 10);
                if (sampleLength < 1)
                {
                    printf("Error: sample time must be at least 1\n");
                    exit(0);
                }
                tflag = 1;
                break;
            case 'r':
                run = (int)strtol(optarg, NULL, 10);
                break;
            case 'b':
                turbo = (int)strtol(optarg, NULL, 10);
                break;
            case ':': //mising arg
                fprintf(stderr, "Error: missing argument\n");
                fprintf(stderr, usage, argv[0]);
                exit(0);
            case'?': //unknown arg
                fprintf(stderr, "Error: unknown parameter \"%c\"\n", optopt);
                fprintf(stderr, usage, argv[0]);
                exit(0);
        }
    }
    if (optind < argc)
    {
        fprintf(stderr, "Error: too many parameters\n");
        fprintf(stderr, usage, argv[0]);
        exit(0);
    }
    if (iflag == 0 || tflag == 0)
    {
        fprintf(stderr, "Error: --interval and --time required\n");
        fprintf(stderr, usage, argv[0]);
        exit(0);
    }

    /* Log file. */
    char hostname[64];
    gethostname(hostname,64);
    char *fname;
    asprintf(&fname, "%s.log.%d_%d", hostname, interval, run);
    FILE *fd = fopen(fname, "a");
    if (fd == NULL)
    {
        fprintf(stderr, "Can't open output file %s!\n", fname);
        exit(1);
    }

    if (turbo == 0)
    {
        disable_turbo();
    }
    if (turbo == 1)
    {
        enable_turbo();
    }

    if (pstate != 0)
    {
        set_each_socket_pstate(pstate);
    }
    if (powerLim != 0 && timewindow == 0)
    {
        dump_power_limits();
        set_each_socket_power_limit(powerLim);
        dump_power_limits();
    }

    if (timewindow != 0)
    {
        dump_power_limits();
        if (powerLim != 0)
        {
            set_each_socket_power_limit_tw(powerLim, timewindow);
        }
        else
        {
            set_each_socket_power_limit_tw(135, timewindow);
        }
        dump_power_limits();
    }
    monitoring(fd, sampleLength, interval, continuous);

    fclose(fd);
    return 0;
}
