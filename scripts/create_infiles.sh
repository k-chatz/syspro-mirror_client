#!/bin/bash

generate_random_string() {
    if [[  $# -ge 1 ]]; then
        maxStringLength=$1
        string="";
        pattern="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        patternLength=`expr length ${pattern}`;
        stringLength=`expr $RANDOM % ${maxStringLength} + 1`
        for ((sl=0; sl <= stringLength; sl++)) do
            ch=`expr $RANDOM % ${patternLength} + 1`;
            string+=`expr substr ${pattern} ${ch} 1`;
        done
    fi
}

validate_args() {
    if [[  $# -ge 4 ]]; then
    shift
        for str; do
            if (! [[ ${1} =~ ^[0-9]+$ ]] ); then
                (>&2 echo "Error: Invalid arguments!")
                exit 1
            fi
            shift
        done
    else
        (>&2 echo "Error: Too few arguments!")
        exit 1
    fi
}

#MAIN

validate_args $@

path=${1};
files=${2};
levels=${4}
dirs=${3}

rm -rf ${path}

mkdir -p ${path}

declare -a paths=()

# Make dirs.
for ((i = 0; i < dirs; i++)); do
    generate_random_string 8

    if [[ ${levels} -le 0 ]]; then
       ((levels=1))
    fi

    depth=`expr ${i} % ${levels}`;

    if [[ ${depth} > 0 ]]; then
        path="${path}/${string}"
    else
        path="$1/${string}"
    fi

    #echo "mkdir -p ${path}"
    mkdir -p ${path}
    paths+=(${path})
done

# Generate files
while [[ ${files} -gt 0 ]]; do
    for p in "${paths[@]}"; do
        generate_random_string 8
        if [[ ${files} > 0 ]]; then
            echo "Create file: ${p}${string}"
            filename=${string}
            echo filename >> "${p}${string}"
            head -c `expr $RANDOM % 128001` /dev/urandom >> "${p}${string}"
            ((files--))
        else
            break;
        fi
    done # for p in "${paths[@]}"
done #while [[ ${files} -gt 0 ]]
