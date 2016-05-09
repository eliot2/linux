#!/bin/sh
set -e
[ -f xt_babblenet.ko ] || make xt_babblenet.ko
sudo modprobe x_tables
sudo insmod xt_babblenet.ko
sudo iptables -t mangle -A INPUT -p tcp --dport 4210 -j LOG
nc -d -k -l 4210 > /dev/null &
