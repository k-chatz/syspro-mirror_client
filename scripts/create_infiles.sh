#!/bin/bash


echo $#

if ! [[ ${2} =~ ^[0-9]+$ ]]
    then
        echo "Sorry integers only"
fi



if [  $# -ge 4 ]; then
	echo command evaluated as TRUE
else
	echo command evaluated as FALSE
fi

