#!/bin/bash

### This script provides input to monitoring_turboLims.sh

MS=10000
nRuns=2

echo "Starting Turbo Limit 28"
turbo=0x1c1c1c1c1c1c1c1c
lim=28
bash monitoring_turboLims.sh $MS $turbo $lim $nRuns

echo "Starting Turbo Limit 29"
turbo=0x1d1d1d1d1d1d1d1d
lim=29
bash monitoring_turboLims.sh $MS $turbo $lim $nRuns

echo "Starting Turbo Limit 30"
turbo=0x1e1e1e1e1e1e1e1e
lim=30
bash monitoring_turboLims.sh $MS $turbo $lim $nRuns

echo "Starting Turbo Limit 31"
turbo=0x1f1f1f1f1f1f1f1f
lim=31
bash monitoring_turboLims.sh $MS $turbo $lim $nRuns

echo "Starting Turbo Limit 32"
turbo=0x2020202020202020
lim=32
bash monitoring_turboLims.sh $MS $turbo $lim $nRuns

echo "Starting Turbo Limit 33"
turbo=0x2121212121212121
lim=33
bash monitoring_turboLims.sh $MS $turbo $lim $nRuns

echo "Starting Turbo Limit 34"
turbo=0x2222222222222222
lim=34
bash monitoring_turboLims.sh $MS $turbo $lim $nRuns

echo "Starting Turbo Limit 35"
turbo=0x2323232323232323
lim=35
bash monitoring_turboLims.sh $MS $turbo $lim $nRuns

echo "Starting Turbo Limit 36"
turbo=0x2424242424242424
lim=36
bash monitoring_turboLims.sh $MS $turbo $lim $nRuns

echo "Starting Turbo Limit 37"
turbo=0x2525252525252525
lim=37
bash monitoring_turboLims.sh $MS $turbo $lim $nRuns

echo "Starting Turbo Limit 38"
turbo=0x2626262626262626
lim=38
bash monitoring_turboLims.sh $MS $turbo $lim $nRuns

cd outFiles
bash process.sh $MS $nRuns

#
