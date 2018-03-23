### pmon: a performance monitoring tool for use with RAPL
Pmon provides an easy interface for monitoring and collecting performance 
data from the model-specific registers (MSRs) on intel platforms.

Pmon is meant to be executed in background on a single core and will monitor the application running on n-1 cores.

The information currently collected by pmon at the socket, core, and thread level:
Instructions Retired
Unhalted Clock Cycles
APERF DELTA
MPERF DELTA
Cache Misses

In addition to performance monitoring, pmom also provides the following functionality:
Request a new initial p-state ratio
Adjust the RAPL lower time window and RAPL lower power limit
Disable or enable turbo

Supported Architectures:
     2A (Sandy Bridge) ** untested
     3E (Ivy Bridge) ** untested
     3E (Haswell)
     4F (Broadwell)
     55 (Skylake) ** untested

#### NOTES
pmon requires that the fixed counter MSRs are fully writable in the whitelist. Other MSRs which are accessed require R/W.


### Additional Scripts
In addition to the c program interface, pmom includes a series of scripts for automating experimental runs and post-processing data.
These can be used as examples and reference for usage of pmon, or utilized for your own experiments.

##### monitoring.sh
./monitoring.sh MS
MS: the interval for sampling, in microseconds

    - adjusts the RAPL power limits and completes a series of experimental runs under the different settings
    - The user must pass the interval for sampling the MSRs through the command line
    - The user should set the appropriate path to the directory that the output files should be saved in
    - The user can replace the FIRESTARTER call with their own application call
    - The amount of time, in seconds, that the monitoring should be done may be adjusted in the pmon argument list. 
      An additional 4 seconds should be added to account for the two sleeps.

example call:
    ./monitoring.sh 10000

##### monitoring_turboLims.sh

./monitoring_turboLims.sh MS HEX DEC numRuns
MS: the interval for sampling, in microseconds
HEX: the max turbo ratio limit, in hex
DEC: the max turbo ratio limit, in base 10
numRuns: the number of experimental runs that should be completed (how many times to execute pmon and the application under the given turbo limit)

    - adjusts the max turbo ratio limit and completes a series of experimental runs
    - requires MSR Tools to be installed
    - The user can replace the FIRESTARTER call with their own application call 
    - The amount of time, in seconds, that the monitoring should be done may be adjusted in the pmon argument list.
      An additional 4 seconds should be added to account for the two sleeps. 
    - Takes command line arguments specifying the interval for sampling, the turbo ratio limit, and the number of experimental runs that should be completed

example call:
    ./monitoring_turboLims.sh 10000 0x1e1e1e1e1e1e1e1e 30 10
     This will launch pmon with a sampling interval of 10000 microseconds, adjust the turbo ratio limit to 30, and run pmon and the application being monitored 10 times

##### turboLim.sh
    - provides input to monitoring_turboLims.sh
    - contains a list of calls, adjusting the max turbo ratio limit each time
    - after all experimental runs are complete, calls the post-processing script contained in the outFiles directory
    
outFiles/process.sh
    - performs post processing of output files from max turbo ratio limit experiment runs
    - the user should look at and refer to the bash script prior to running, as the turbo ratio limit array may need to be adjusted
sample call:
    ./process.sh 10000 10
