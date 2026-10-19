#!/bin/bash

make redo
rm -rf *.csv

path=/media/micaela/DATA/FUNTIMES2/Datasets/DATI_CHIMERA/CHIE_20200623/CHIE/Output/
folder=Test05_df
slash=/

pathResults=/home/micaela/ChimeraProfiling/DF



df=7
if [ $# -gt 0 ]; then
    iterations=$1
else
    iterations=5
    echo "No input parameters specified, default number of iterations is used"
fi
echo "$iterations iterations will be executed"

for (( k=1; k<=($df); k++ ))
do
	echo "    $path$folder$k$slash"
	for (( i=1; i<=($iterations); i++ ))
	do
	    echo "    $i of $iterations..."
	    ./testCrypto $path$folder$k$slash > log.txt
	    rm -rf *.bin
	    rm -rf *.txt
	done
        mv profilingResults.csv $pathResults$k$slash
done
echo 'Finished!'
