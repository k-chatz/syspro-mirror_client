#!/bin/bash

validate_args() {
    if [  $# -ge 4 ]; then
    shift
        for str
            do
            if (! [[ ${1} =~ ^[0-9]+$ ]] ); then
                (>&2 echo "Error: Invalid arguments!")
                exit 1
            fi
            shift +1
            done
    else
        (>&2 echo "Error: Too few arguments!")
        exit 1
    fi
}

echo "Number of arguments: [$#]"

validate_args $@

mkdir $1; cd $1;


