#!/bin/bash -e

die () {
    echo "$1" >&2
    exit 1
}

[ "$#" == 2 ] || die "Usage: $0 INPUT OUTPUT"

sz=$(stat --printf="%s" "$1")
sectors=$(((sz + 511) / 512))
echo "$sectors sectors"

[ "$sectors" -le 36 ] || die "Binary too large ($sectors sectors)"

osz=$((sectors * 512))
dd if="$1" of="$2" bs=512
dd if=/dev/null of="$2" bs=1 count=1 seek="$osz"
