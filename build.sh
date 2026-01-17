#!/bin/bash

source /usr/local/qnx/env/bin/activate
source /home/joshuadeering/qnx800/qnxsdp-env.sh

QNX_HOST=/home/joshuadeering/qnx800/host/linux/x86_64
QNX_TARGET=/home/joshuadeering/qnx800/target/qnx

cd /home/joshuadeering/qnxprojects/uOttaHack/

ntoaarch64-g++ -I$QNX_TARGET/usr/include -Igpio/aarch64/ \
    -o uOttaHack uOttaHack.cpp \
    -L$QNX_TARGET/usr/lib -lc++ -lc