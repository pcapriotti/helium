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

if [[ -n "$NET_TAP" ]]; then
    opt_net=" -netdev tap,ifname=tap0,script=no,downscript=no,id=n1"
    opt_net="$opt_net -device rtl8139,netdev=n1,id=nic1"
fi

if [[ -n "$MULTIBOOT" ]]; then
    opt_mb="-kernel build/kernel/kernel.elf"
fi

$QEMU -serial $SERIAL \
      -m $MEM \
      -drive id=disk,file=build/disk.img,if=none \
      $opt_ahci \
      -device "ide-drive,drive=disk$opt_bus" \
      $opt_net \
      $opt_mb \
      "$@"
