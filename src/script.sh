#!/bin/bash

# if script launched without options or -help it outputs use message
if [[ $# -eq 2 ]]; then
    echo "--- $0 in uso ---"
else
    for i in $@; do
        if [[ $i == "-help" ]]; then
            echo "--- $0 in uso ---"
        fi
    done
fi 

# checking second parameter >= 0
if [ "$2" -lt 0 ]; then
    echo "usa $0 chatty.conf pos_int"
    exit 1
fi

filename=$1

# checking first parameter (chatty.conf) not irregular
if [ ! -f $filename ]; then
    echo "il file $filename non esiste o non e' un file regolare"
    exit 1
fi

# extracting directory name associated to DirName and removing spaces 
while IFS== read a b; do
    if [[ "$a" == "DirName          " ]]; then
        var=$b
    fi
done < $filename

var="${var//" "}"

# if t==0 outputs all DirName files, if t>0 remove DirName files older than t minutes
if [[ $2 -eq 0 ]]; then
    echo "Files in $var:"
    for entry in "$var"/*
    do
        echo "${entry//"$var/"}"
    done
else
    find "$var"/* -mmin +$2 -delete
fi