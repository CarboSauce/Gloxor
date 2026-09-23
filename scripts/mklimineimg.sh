#!/usr/bin/env bash
# 
# arg[1] = kernel file
# arg[2] = out directory
# arg[3] = limine directory
#
set -x
set -e

if [ "$#" -ne 3 ]; then
    echo "Scripts requires 3 arguments"
    exit 1
fi

if [ ! -f "$2"/limine.conf ]; then
    echo "$2 directory needs to contain limine.cfg before building an iso image"
    exit 1
fi

mkdir -p "$2"/isoroot/boot/limine

cp "$2/limine.conf" "$3/limine-bios.sys" "$3/limine-bios-cd.bin" "$3/limine-uefi-cd.bin" \
    "$2/isoroot/boot/limine"
cp "$1" "$2/isoroot/boot"
xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        "$2/isoroot" -o "$2"/os.iso
"$3"/limine bios-install "$2"/os.iso
