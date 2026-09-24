#!/usr/bin/env bash
#
# arg[1] = qemu command
# arg[2] = kernel image
# arg[3]? = additional arguments to qemu 0

set -e

qemu="$1"
kernel="$2"

if [[ -z "$qemu" || -z "$kernel" ]]; then
    echo "usage: $0 <qemu-command> <kernel-image> [qemu-args...]" >&2
    exit 2
fi

# Try to resolve the kernel path through WSL first.
if command -v wslpath >/dev/null 2>&1; then
    if resolved_kernel="$(wslpath -w "$kernel" 2>/dev/null)"; then
        kernel="$resolved_kernel" 
    fi 
fi

eval "args=(${@:3})"

exec "$qemu" -hda "$kernel" "${args[@]}"

