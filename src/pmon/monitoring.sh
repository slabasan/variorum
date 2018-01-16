#!/bin/bash

VARIORUM_DIR=$HOME/libvariorum/build/variorum
PMON_DIR=$HOME/libvariorum/build/pmon

if [ "$#" -ne 1 ]; then
    echo "Error: must provide interval for sampling in microseconds"
    exit 1
fi

MS=$1
## MS units is microseconds. This is the interval for sampling.

turbo_lim=0x1C1C1C1C1C1C1C1C
turbo_limi=0x1C1C
#echo $turbo_lim

echo "0x1AD"
$HOME/rdmsr 0x1AD -p 0
$HOME/rdmsr 0x1AD -p 1
$HOME/wrmsr 0x1AD -p 0 $turbo_lim 
$HOME/wrmsr 0x1AD -p 1 $turbo_lim
$HOME/rdmsr 0x1AD -p 0
$HOME/rdmsr 0x1AD -p 1
echo "0x1AE"
$HOME/rdmsr 0x1AE -p 0
$HOME/rdmsr 0x1AE -p 1 
$HOME/wrmsr 0x1AE -p 0 $turbo_lim 
$HOME/wrmsr 0x1AE -p 1 $turbo_lim
$HOME/rdmsr 0x1AE -p 0
$HOME/rdmsr 0x1AE -p 1
echo "0x1AF"
$HOME/rdmsr 0x1AF -p 0
$HOME/rdmsr 0x1AF -p 1 
$HOME/wrmsr 0x1AF -p 0 $turbo_limi 
$HOME/wrmsr 0x1AF -p 1 $turbo_limi
$HOME/rdmsr 0x1AF -p 0
$HOME/rdmsr 0x1AF -p 1
#pstates=( 36 35 34 32 30 28 )
#for p in "${pstates[@]}"; do
for i in {1..1}; do
    taskset -c 0,16 env LD_LIBRARY_PATH=${VARIORUM_DIR}/lib:${LD_LIBRARY_PATH} ${PMON_DIR}/pmon -i $MS -t 10 -r $i -b 1 &
    pid=$(echo $!)
    sleep 2s
    
    date >> FS_out
    taskset -c 1-15,17-31 ${HOME}/FIRESTARTER2/FIRESTARTER -t 6 -r > FS_out
    sleep 2s
    wait $pid

    file=$(ls *.log*)
    mv $file $HOME/${file}_${turbo_lim}
   # mv FS_out /home/connors6/libvariorum/src/pmon/FS_out_${MS}_${i}_${turbo_lim}
done
#done


