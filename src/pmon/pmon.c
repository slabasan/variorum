// Program to monitor performance of a running application and make adjustments
// to RAPL. Runs on a dedicated core
//
// Author: Tiffany A. Connors 

#define _GNU_SOURCE

#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <variorum.h>

int main(int argc, char **argv)
{
    int interval, sampleLength, c;
    int run = 0, pstate = 0;
    double timewindow = 0;
    int continuous = 0;
    int tflag = 0, iflag = 0;
    int turbo = -1;

    static struct option long_options[] = {
        {"help",	   no_argument,       0, 'h'},
        {"interval",   required_argument, 0, 'i'},
        {"time", 	   required_argument, 0, 't'},
        {"runNum",     required_argument, 0, 'r'},
        {"pstate",	   required_argument, 0, 'p'},
        {"timeWindow", required_argument, 0, 'w'},
        {"turbo",	   required_argument,  	  0, 'b'},
        {0,            0,                 0,  0 }
    };

    while ((c = getopt_long(argc, argv, "hi:t:r:p:w:b:", long_options, NULL)) != -1)
    {
        switch(c)
        {
        case 'h':
            printf("\nUsage: monitor [args]\n"
		           "\nArgs/Options:\n\n"
                   "-h | --help \t\t display this message\n"
		           "-i | --interval \t Required. How often to sample, in microseconds\n"
		           "				   Pass 0 to sample continuously\n"
		           "-t | --time \t\t Required. The length of time to do sampling, in seconds\n"
		           "-r | --runNum \t\t Optional. The iteration number of experimental runs\n"
                   "-p | --pstate \t\t Optional. Request a new p-state\n"
                   "-w | --timeWindow \t Optional. Change RAPL time window\n"
                   "-b | --turbo \t\t Optional. 0 to disable and 1 to enable \n\n");
           exit(0);
           break;
        case 'p':
            pstate = (int)strtol(optarg, NULL,10);
            break;
        case 'w':
            timewindow=(double)strtol(optarg, NULL, 10);
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
                printf("Error: sample time must be atleast 1\n");
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
	        exit(0);
        case'?': //unknown arg
            exit(0);
        }
    }
    if (optind < argc)
    {
        printf("Error: too many parameters\n");
        exit(0);
    }
    if (iflag == 0 || tflag == 0)
    {
        printf("Error: --interval and --time required\n");
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
    if(turbo == 1)
    {
        enable_turbo();
    } 

    if (pstate != 0)
    {
        set_each_pstate(pstate);
    }
    if (timewindow != 0)
    {
        set_each_socket_power_limit_tw(130, timewindow);
        dump_power_limits();
    }
    monitoring(fd, sampleLength, interval, continuous);  

    fclose(fd);
    return 0;
}
