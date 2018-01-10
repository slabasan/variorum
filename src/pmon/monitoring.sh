#!/bin/bash

VARIORUM_DIR=$HOME/cz_stash/libvariorum/install-connors6
PMON_DIR=$HOME/cz_stash/libvariorum/build-connors6/pmon

if [ "$#" -ne 1 ]; then
    echo "Error: must provide interval for sampling in microseconds"
    exit 1
fi

MS=$1
## MS units is microseconds. This is the interval for sampling.

for i in {0..1}; do
    taskset -c 15,31 env LD_LIBRARY_PATH=${VARIORUM_DIR}/lib:${LD_LIBRARY_PATH} ${PMON_DIR}/pmon -i $MS -t 12 -r $i -b $i &
    pid=$(echo $!)
    sleep 1s

    taskset -c 0-14,16-30 ${HOME}/FIRESTARTER2/FIRESTARTER -t 6 -r -s 3 > /dev/null
    wait $pid
done

