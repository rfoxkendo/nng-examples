#!/bin/bash

# Script to gather all the timing for  the bus:

echo Bus timing log > bus.log

for service in tcp://localhost:300%d ipc:///tmp/bus%d inproc://bus%d
do      
	       echo ----    $service timings -------- >> bus.log

for size in 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576
do
    for members in 2 3 4 5
    do
	echo msg size $size members $members >>bus.log
	./bus $service  10000 $size $members >> bus.log
    done
done
done
		   
