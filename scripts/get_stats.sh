#!/bin/bash

clients=0
bs=0
br=0
fs=0
fr=0
dn=0

while read line; do
    cmd=`echo ${line} | cut -d' ' -f1`
    value=`echo ${line} | cut -d' ' -f2`

    case "${cmd}" in

    cl)
        ((clients++))
        #todo: add client in array
        ;;
    bs)
        ((bs+=value))
        ;;
    br)
        ((br+=value))
        ;;
    fs)
        ((fs+=value))
        ;;
    fr)
        ((fr+=value))
        ;;
    dn)
        ((dn++))
        ;;
    esac

done < /dev/stdin

echo "Clients: ${clients}"
echo "Minimum client id: ${clients}" #todo: find min value from array
echo "Maximum client id: ${clients}" #todo: find max value from array
echo "Sent bytes: ${bs}"
echo "Receive bytes: ${br}"
echo "Sent files: ${fs}"
echo "Receive files: ${fr}"
echo "Clients who leave: ${dn}"
