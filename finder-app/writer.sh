#!/bin/sh

writefile="$1"
writestr="$2"

if [ $# -lt 2 ]
then
    echo "Usage: writer.sh <file> <string>"
    exit 1
else 
    # chek if writefile is null
    if [ -z "$writefile" ]
    then 
        echo "File path is null"
        exit 1
    fi
    # check if writestr is null
    if [ -z "$writestr" ]
    then
        echo "Write string is null"
        exit 1
    fi
fi

dirpath=$(dirname "$writefile")
if [ ! -d "$dirpath" ]
then
    mkdir -p "$dirpath"
    if [ $? -ne 0 ]
    then
        echo "Failed to create directory $dirpath"
        exit 1
    fi
fi

echo "$writestr" > "$writefile"
