#!/bin/bash -e

: ${MEM:=2G}
tup
qemu-system-i386 -hda build/disk.img -serial stdio -m $MEM
