#!/bin/bash -e

tup

: ${MEM:=2G}
: ${QEMUSYS:=i386}
: ${QEMU:=qemu-system-$QEMUSYS}
: ${SERIAL:=stdio}

if [[ -n "$AHCI" ]]; then
    opt_ahci="-device ahci,id=ahci"
    opt_bus=",bus=ahci.0"
fi

$QEMU -serial $SERIAL \
      -m $MEM \
      -drive id=disk,file=build/disk.img,if=none \
      $opt_ahci \
      -device "ide-drive,drive=disk$opt_bus"
