#!/bin/bash -e

parse_size() {
    if [[ "$1" == *s ]]
    then
        echo $((${1%s} * 512))
    elif [[ "$1" == *k ]]
    then
        echo $((${1%k} * 1024))
    elif [[ "$1" == *M ]]
    then
        echo $((${1%M} * 1024 * 1024))
    elif [[ "$1" == *G ]]
    then
        echo $((${1%G} * 1024 * 1024 * 1024))
    else
        echo "$1"
    fi
}

usage() {
    echo "Usage: $0 [-s SIZE] [-o OFFSET] DISK DIR" >&2
    exit 1
}

while getopts "s:o:l:" x; do
    case $x in
        s) size=$(parse_size "$OPTARG");;
        o) offset=$(parse_size "$OPTARG");;
        l) loader="$OPTARG";;
    esac
done
shift $((OPTIND - 1))
disk=$1
src=$2
[ -z "$3" ] || usage

: ${size:=$((3 * 1024 * 1024))}
: ${offset:=$((72 * 512))}
: ${disk:="disk.img"}
: ${src:="image"}

dd if=/dev/zero of="$disk" bs=1024 count=$(((size + 1023) / 1024)) >&2
parted -m -s "$disk" -- mklabel msdos >&2
parted -m -s "$disk" -- mkpart primary ext2 "${offset}B" "-1s" >&2
mke2fs -d "$src" -t ext2 -q -F -F -E offset="$offset",no_copy_xattrs "$disk" "$(((size - offset) / 1024))" >&2

if [[ -n "$loader" ]]; then
    echo "writing mbr"
    dd if="$loader" of="$disk" bs=1 count=440 conv=notrunc,nocreat
    echo "copying loader"
    dd if="$loader" of="$disk" bs=512 conv=notrunc,nocreat seek=1 skip=1
fi
