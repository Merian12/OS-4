#!/bin/bash

for i in $(seq 1 20);
do
    ./search shakespeare-full.txt "Something is rotten in the state of Denmark." 8 $i dataT.csv
done
