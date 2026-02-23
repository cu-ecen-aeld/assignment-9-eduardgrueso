#!/bin/sh

# root directory path
filesdir=$1

# string to search
searchstr=$2

if [ $# -lt 2 ]
then
    echo "Usage: finder.sh <directory> <string>"
    exit 1
else 
    # chek if filesdir is null
    if [ -z "$filesdir" ]
    then 
        echo "Directory path is null"
        exit 1
    fi
    # check if searchstr is null
    if [ -z "$searchstr" ]
    then
        echo "Search sting is null"
        exit 1
    fi
    #check if filesdir exists on the filesystem
    if [ ! -d "$filesdir" ]
    then
        echo "Directory path does not exist"
        exit 1
    fi
fi


#find the string on the root directory

number_of_files=$(grep -rl "$searchstr" "$filesdir" 2>/dev/null | wc -l)
number_of_lines=$(grep -ro "$searchstr" "$filesdir" 2>/dev/null | wc -l)

echo "The number of files are $number_of_files and the number of matching lines are $number_of_lines"