#!/bin/bash

algm="$1"

if [ "$algm" == "" ]; then
    echo "usage: $0 [fifo|lru|clock]"
    exit
fi

# First 5 hex values are the page #
# Last 3 hex values are the offset
./virtmem --file="in_${algm}.txt" --framesize=12 --numframes=4 --replace=$algm #--progress
