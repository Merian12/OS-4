#!/bin/bash

for i in $(seq 1 5);
do
    ./search shakespeare-full.txt "Something is rotten in the state of Denmark." $i 8 results.csv
done
