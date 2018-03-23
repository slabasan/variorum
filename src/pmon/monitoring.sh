#!/bin/bash

VARIORUM_DIR=$HOME/libvariorum/build/variorum
PMON_DIR=$HOME/libvariorum/build/pmon
#### User should set appropriate path to directory for output files
OUTPUT_DIR=$HOME/libvariorum/src/pmon/outfiles 

if [ "$#" -ne 1 ]; then
    echo "Error: must provide interval for sampling in microseconds"
    exit 1
fi

MS=$1

### Change the RAPL power limits, as listed in the array
limits=(60 80 100 120 140) # Array of power limits, in watts. Can adjust array contents as needed (add or remove power limits)
for l in "${limits[@]}"; do
    taskset -c 0,16 env LD_LIBRARY_PATH=${VARIORUM_DIR}/lib:${LD_LIBRARY_PATH} ${PMON_DIR}/pmon -i $MS -t 1 -l $l &
    pid1=$(echo $!)
    wait $pid1
    rm curry.log.${MS}_0

# run the program and collect performance data for a given number of iterations
    for i in {1..50}; do
        echo $i
        
        # launch pmon in the background on a single core (use both threads on the core if HT is enabled)
        taskset -c 0,16 env LD_LIBRARY_PATH=${VARIORUM_DIR}/lib:${LD_LIBRARY_PATH} ${PMON_DIR}/pmon -i $MS -t 10 -r $i &
        pid=$(echo $!)
        sleep 2s

        # execute application to be monitored, binding to n-1 cores/ n-2 threads if HT is enabled
        taskset -c 1-15,17-31 ${HOME}/FIRESTARTER2/FIRESTARTER -t 6 -r >> FS_out
        sleep 2s
        wait $pid

        mv $file ${OUTPUT_DIR}/${file}_${lim}_${l}
        mv FS_out ${OUTPUT_DIR}/FS_out_${MS}_${i}_${lim}_${l}
    done
done


#################### RESET TO DEFUALTS ####################
echo "Resetting system defaults"

#reset powLim to TDP
taskset -c 0,16 env LD_LIBRARY_PATH=${VARIORUM_DIR}/lib:${LD_LIBRARY_PATH} ${PMON_DIR}/pmon -i $MS -t 1 -l 135 & > /dev/null
pid1=$(echo $!)
wait $pid1
rm curry.log.${MS}_0
