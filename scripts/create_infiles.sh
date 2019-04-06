#!/bin/bash

validate_args() {
    if [[  $# -ge 4 ]]; then
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

#MAIN

echo "Number of arguments: [$#]"

validate_args $@

mkdir $1; cd $1;


#echo $(($RANDOM % 2))

#rand=$(echo $RANDOM)
#echo $rand
#
#string=''
#
# for i in {0..1}
#  do
# string+=$(printf "%c" 114 )
#
# done
#echo $string


# if [[ "$?" != 0 ]]; then
#        echo "can't acquire lock!";
# fi