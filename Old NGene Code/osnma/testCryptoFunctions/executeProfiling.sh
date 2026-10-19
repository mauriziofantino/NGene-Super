#!/bin/bash

make redo
rm -rf *.csv

if [ $# -gt 0 ]; then
    iterations=$1
else
    iterations=5
    echo "No input parameters specified, default number of iterations is used"
fi
echo "$iterations iterations will be executed"


for (( i=1; i<=($iterations); i++ ))
do
    echo "    $i of $iterations..."
    ./bin/ngene2 -t > log.txt
    rm -rf *.bin
    rm -rf *.txt
done
echo 'Finished!'
