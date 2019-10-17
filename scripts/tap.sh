#!/bin/bash
# set up tap interface for testing with qemu

name="${1:-tap0}"

ip tuntap add name "$name" mode tap
ip link set "$name" up
ip addr add 192.168.5.1 dev "$name"
ip route add 192.168.5.0/24 dev "$name"
