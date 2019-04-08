#!/bin/bash

clients=0
bs=0
br=0
fs=0
fr=0
dn=0

declare -a cls=()

while read line; do
    cmd=`echo ${line} | cut -d' ' -f1`
    value=`echo ${line} | cut -d' ' -f2`
    case "${cmd}" in
    cl)
        ((clients++))
         cls+=(${value})
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

max=${cls[0]}
min=${cls[0]}

for i in "${cls[@]}"; do
    if [[ ${i} -gt ${max} ]]; then
      max=${i}
    fi
    if [[ ${i} -lt ${min} ]]; then
      min=${i}
    fi
done

echo "Clients: ${clients} (${cls[@]})"
echo "Minimum client id: ${min}"
echo "Maximum client id: ${max}"
echo "Sent bytes: ${bs}"
echo "Receive bytes: ${br}"
echo "Sent files: ${fs}"
echo "Receive files: ${fr}"
echo "Clients who leave: ${dn}"
