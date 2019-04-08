#!/bin/bash

while read line; do
    cmd=`echo ${line} | cut -d' ' -f1`
    value=`echo ${line} | cut -d' ' -f2`

    case "${cmd}" in

    cl) echo "cl command ${value}"

        ;;
    bs) echo "bs command ${value}"

        ;;
    br) echo "br command ${value}"

        ;;
    fs) echo "fs command ${value}"

        ;;
    fr) echo "fr command ${value}"

        ;;
    dn) echo "dn command ${value}"

        ;;
    esac

done < /dev/stdin
