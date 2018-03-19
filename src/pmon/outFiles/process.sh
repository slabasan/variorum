#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Usage: ./process.sh MS numRuns"
    exit 1
fi

MS=$1
nruns=$2
host=$(hostname)
lims=( 28 29 30 31 32 33 34 35 36 37 38 ) #the turbo ratio limits. Should match the ones used in experiment. Adjust as needed.

for n in "${lims[@]}"; do
    echo $n
    for i in $( seq 1 ${nruns} ); do
        iter=$(grep "total iterations:" FS_out_${MS}_${i}_${n} | awk '{print $3}')
        awk '{print $0 ","'${iter}'","'${n}'","'${i}'}' ${host}.log.${MS}_${i}_${n} >> ${host}log_${MS}_${i}_${n}
        head -16 ${host}log_${MS}_${i}_${n} >> ${host}_end_to_end_${n}_${i}
        tail -16 ${host}log_${MS}_${i}_${n} >> ${host}_end_to_end_${n}_${i}
    done

    rm FS_out_${MS}_*_${n}
    rm ${host}.log.${MS}_*_${n}

    cat ${host}log_${MS}_*_${n} >> ${host}_${n}_${MS}

    printf "Socket,Core,Thread,Time,InstRet,UnhaltClkCycles,APERF_DELTA,MPERF_DELTA,cacheMiss,iterations,turboCap,run\n" | cat - ${host}_${n}_${MS} > temp && mv -f temp ${host}_${n}_${MS}
    printf "Socket,Core,Thread,Time,InstRet,UnhaltClkCycles,APERF_DELTA,MPERF_DELTA,cacheMiss,iterations,turboCap,run\n" | cat - ${host}_end_to_end_${n}_${i} > temp 
    mv -f temp ${host}_end_to_end_${n}_${i}

done
