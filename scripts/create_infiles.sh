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
            shift
            done
    else
        (>&2 echo "Error: Too few arguments!")
        exit 1
    fi
}

#MAIN

echo "Number of arguments: [$#]"

validate_args $@

# Random characters.
x="abcdefghijklwricoalc234019495023";

# Make input_dir recursively.
mkdir -p $1;

ch="";
if [[ $? -eq 0 ]]; then
    cd $1;
	strlen=`expr length "$x"`;
    characters=`expr $RANDOM % 8 + 1`;
    for ((i=0; i <= $characters; i++)) do
        c=`expr $RANDOM % $strlen + 1`;
        ch+=`expr substr $x $c 1`;
        echo "$i: $ch";
    done
    echo $ch
fi


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