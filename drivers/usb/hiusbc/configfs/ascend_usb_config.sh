#!/bin/bash
#—————————————————————————-
# Purpose: usb for ether or udisk
# Copyright Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
# Author:huawei
#—————————————————————————-
set -e
usb_id=$1
if [ $usb_id == 1 ]; then
    u1=`cat /sys/kernel/config/usb_gadget/g1/UDC 2>/dev/null`
    if [ "${u1}" != "a5080000.hiusbc1" ];then
        echo "a5080000.hiusbc1" > /sys/kernel/config/usb_gadget/g1/UDC
    fi
elif [ $usb_id == 2 ]; then
    u2=`cat /sys/kernel/config/usb_gadget/g2/UDC 2>/dev/null`
    if [ "${u2}" != "a5100000.hiusbc2" ];then
        echo "a5100000.hiusbc2" > /sys/kernel/config/usb_gadget/g2/UDC
    fi
elif [ $usb_id == 3 ]; then
    u3=`cat /sys/kernel/config/usb_gadget/g3/UDC 2>/dev/null`
    if [ "${u3}" != "a5180000.hiusbc3" ];then
        echo "a5180000.hiusbc3" > /sys/kernel/config/usb_gadget/g3/UDC
    fi
fi