#!/bin/bash

#### This script automates the data collection experimental runs.
#### Supports Turbo Ratio Limit modification with and without setting power limits

VARIORUM_DIR=$HOME/libvariorum/build/variorum
PMON_DIR=$HOME/libvariorum/build/pmon
MSR_TOOLS_DIR=$HOME/msr-tools-1.1

if [ "$#" -ne 4 ]; then
    echo "usage: ./monitoring.sh <sample rate in microseconds> <turbo limit in hex> <turbo limit in dec> <num runs>"
    echo "sample rate: how often to take samples in microseconds"
    echo "turbo limit in hex: the hex value for setting the max turbo ratio limit"
    echo "turbo limit in dec: the value for the max turbo ratio limit in base 10"
    echo "num runs: the number of times the program should be run and data collected" 
    exit 1
fi

MS=$1
## MS units is microseconds. This is the interval for sampling.
turbo_lim=$2
lim=$3
nRuns=$4
host=$(hostname)

############# Set new values for turbo ratio limit in register ####################
echo "0x1AD"
for p in {0..15}; do
    ${MSR_TOOLS_DIR}/rdmsr 0x1AD -p $p
    ${MSR_TOOLS_DIR}/wrmsr 0x1AD -p $p $turbo_lim
    nlim=$(${MSR_TOOLS_DIR}/rdmsr 0x1AD -p $p)
    if [ "0x$nlim" != "$turbo_lim" ]; then
        echo "ERROR: Limits don't match!"
        exit
    fi
done

echo "0x1AE"
for p in {0..15}; do
    ${MSR_TOOLS_DIR}/rdmsr 0x1AE -p $p
    ${MSR_TOOLS_DIR}/wrmsr 0x1AE -p $p $turbo_lim
    nlim=$(${MSR_TOOLS_DIR}/rdmsr 0x1AE -p $p)
    if [ "0x$nlim" != "$turbo_lim" ]; then
        echo "ERROR: Limits don't match!"
        exit
    fi
done

####### For Haswell, only need to set the semaphone bit. Leaving original value in remaining bits
echo "0x1AF"
for p in {0..15}; do
    ${MSR_TOOLS_DIR}/rdmsr 0x1AF -p $p
    ${MSR_TOOLS_DIR}/wrmsr 0x1AF -p $p 0x8000000000002222
    ${MSR_TOOLS_DIR}/rdmsr 0x1AF -p $p
done

################# Set power limits ######################
#limits=( 60 80 100 120 140 )
#for l in "${limits[@]}"; do
#taskset -c 0,16 env LD_LIBRARY_PATH=${VARIORUM_DIR}/lib:${LD_LIBRARY_PATH} ${PMON_DIR}/pmon -i $MS -t 1 -l $l &
#pid1=$(echo $!)
#wait $pid1
#rm ${host}.log.${MS}_0

######################### Run Benchmark and Monitoring Program for a set number of times #######################
for i in $(seq 1 $nRuns); do
    echo $i
    ### Launch monitoring in background on 1 core
    taskset -c 0,16 env LD_LIBRARY_PATH=${VARIORUM_DIR}/lib:${LD_LIBRARY_PATH} ${PMON_DIR}/pmon -i $MS -t 10 -r $i &
    pid=$(echo $!)
    sleep 2s

    ### Run FS on N-1 cores
    taskset -c 1-15,17-31 ${HOME}/FIRESTARTER2/FIRESTARTER -t 6 -r >> FS_out
    sleep 2s
    wait $pid

    file=$(ls ${host}.log*)
    mv $file outFiles/${file}_${lim}
    mv FS_out outFiles/FS_out_${MS}_${i}_${lim}


    ############# Check that the register values weren't reset ####################
    for p in {0..15}; do
        nlim=$(${MSR_TOOLS_DIR}/rdmsr 0x1AD -p $p)
        if [ "0x$nlim" != "$turbo_lim" ]; then
            echo "ERROR: Limits don't match!"
            exit
        fi
    done
done
#done

#################### RESET TO DEFUALTS ####################
echo "Resetting system defaults"
#reset powLim to TDP
taskset -c 0,16 env LD_LIBRARY_PATH=${VARIORUM_DIR}/lib:${LD_LIBRARY_PATH} ${PMON_DIR}/pmon -i $MS -t 1 -l 135 & > /dev/null
pid1=$(echo $!)
wait $pid1
rm ${host}.log.${MS}_0

#reset max turbo ratio
for p in {0..15}; do
    ${MSR_TOOLS_DIR}/wrmsr 0x1AD -p $p 0x2222222222222424
done

for p in {0..15}; do
    ${MSR_TOOLS_DIR}/wrmsr 0x1AE -p $p 0x2222222222222222
done

for p in {0..15}; do
    ${MSR_TOOLS_DIR}/wrmsr 0x1AF -p $p 0x8000000000002222
done

#
