#!/bin/bash
echo "# This file was generated by running the bin2list.sh script in bluebombs stage0 folder"
echo ""
for file in *_*_*.bin; do
    name=$(basename $file .bin)
    value=$(od -An -tx4 --endian=big $file | xargs)
    echo "set($name 0x$value)"
    echo "list(APPEND BLUEBOMB_L2CB_LIST $name)"
done