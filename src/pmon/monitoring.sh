#!/bin/bash

VARIORUM_DIR=$HOME/libvariorum/build/variorum
PMON_DIR=$HOME/libvariorum/build/pmon

if [ "$#" -ne 1 ]; then
    echo "Error: must provide interval for sampling in microseconds"
    exit 1
fi

MS=$1
## MS units is microseconds. This is the interval for sampling.

#turbo_lim=0x73A3A3A3A3A3A3A3A
turbo_lim=0x2525252525252525
lim=37

echo "0x1AD"
for p in {0..15}; do
    /home/connors6/msr-tools-1.1/rdmsr 0x1AD -p $p
    /home/connors6/msr-tools-1.1/wrmsr 0x1AD -p $p $turbo_lim
    /home/connors6/msr-tools-1.1/rdmsr 0x1AD -p $p
done

echo "0x1AE"
for p in {0..15}; do
    /home/connors6/msr-tools-1.1/rdmsr 0x1AE -p $p
    /home/connors6/msr-tools-1.1/wrmsr 0x1AE -p $p $turbo_lim
    /home/connors6/msr-tools-1.1/rdmsr 0x1AE -p $p
done

echo "0x1AF"
for p in {0..15}; do
    /home/connors6/msr-tools-1.1/rdmsr 0x1AF -p $p
    /home/connors6/msr-tools-1.1/wrmsr 0x1AF -p $p 0x8000000000002525
    /home/connors6/msr-tools-1.1/rdmsr 0x1AF -p $p
done

limits=(60 80 100 120 140)
for l in "${limits[@]}"; do
    taskset -c 0,16 env LD_LIBRARY_PATH=${VARIORUM_DIR}/lib:${LD_LIBRARY_PATH} ${PMON_DIR}/pmon -i $MS -t 1 -l $l &
    pid1=$(echo $!)
    wait $pid1
    rm curry.log.${MS}_0

    for i in {1..50}; do
        echo $i
        taskset -c 0,16 env LD_LIBRARY_PATH=${VARIORUM_DIR}/lib:${LD_LIBRARY_PATH} ${PMON_DIR}/pmon -i $MS -t 10 -r $i &
        pid=$(echo $!)
        sleep 2s

        date >> FS_out
        taskset -c 1-15,17-31 ${HOME}/FIRESTARTER2/FIRESTARTER -t 6 -r >> FS_out
        sleep 2s
        wait $pid

        file=$(ls curry.log*)
        mv $file /home/connors6/libvariorum/src/pmon/powCap_results/${file}_${lim}_${l}
        mv FS_out /home/connors6/libvariorum/src/pmon/powCap_results/FS_out_${MS}_${i}_${lim}_${l}
    done
done


#################### RESET TO DEFUALTS ####################
echo "Resetting system defaults"

#reset powLim to TDP
taskset -c 0,16 env LD_LIBRARY_PATH=${VARIORUM_DIR}/lib:${LD_LIBRARY_PATH} ${PMON_DIR}/pmon -i $MS -t 1 -l 135 & > /dev/null
pid1=$(echo $!)
wait $pid1
rm curry.log.${MS}_0

#reset max turbo ratio
for p in {0..15}; do
    /home/connors6/msr-tools-1.1/wrmsr 0x1AD -p $p 0x2222222222222424
done

for p in {0..15}; do
    /home/connors6/msr-tools-1.1/wrmsr 0x1AE -p $p 0x2222222222222222
done

for p in {0..15}; do
    /home/connors6/msr-tools-1.1/wrmsr 0x1AF -p $p 0x8000000000002222
done

#
