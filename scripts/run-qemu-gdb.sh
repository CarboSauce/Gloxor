#!/usr/bin/env bash
# Usage:
# run-qemu-gdb.sh <qemu> <iso-image> <kernel-elf> [qemu-args...]

set -e

qemu="$1"
iso="$2"
elf="$3"
shift 3

gdb_command="${GDB_COMMAND:-gdb}"
gdb_port="${GDB_PORT:-1234}"

if [[ -z "$qemu" || -z "$iso" || -z "$elf" ]]; then
    echo "usage: $0 <qemu-command> <iso-image> <kernel-elf> [qemu-args...]" >&2
    exit 2
fi

# Resolve ISO path through WSL if available.
if command -v wslpath >/dev/null 2>&1; then
    if resolved_iso="$(wslpath -w "$iso" 2>/dev/null)"; then
        iso="$resolved_iso"
    fi
fi

qemu_args=(
    -cdrom "$iso"
    -boot d
    -gdb "tcp::$gdb_port"
    -S
    "$@"
)

echo "Starting QEMU..."
echo "GDB server: localhost:$gdb_port"

"$qemu" "${qemu_args[@]}" &
qemu_pid=$!

cleanup() {
    kill "$qemu_pid" 2>/dev/null || true
    wait "$qemu_pid" 2>/dev/null || true
}

trap cleanup EXIT INT TERM

echo "Starting GDB..."

"$gdb_command" "$elf" \
    -ex "target remote localhost:$gdb_port"
