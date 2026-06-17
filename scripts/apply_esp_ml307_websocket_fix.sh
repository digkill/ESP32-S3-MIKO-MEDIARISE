#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
patch_file="${root_dir}/patches/esp-ml307-websocket-wdt.patch"

if [[ ! -f "${root_dir}/managed_components/78__esp-ml307/src/web_socket.cc" ]]; then
    exit 0
fi

if patch --dry-run --silent --forward -p1 -d "${root_dir}" < "${patch_file}" >/dev/null 2>&1; then
    patch --silent --forward -p1 -d "${root_dir}" < "${patch_file}"
elif patch --dry-run --silent --reverse -p1 -d "${root_dir}" < "${patch_file}" >/dev/null 2>&1; then
    exit 0
else
    echo "Unable to apply ${patch_file}" >&2
    exit 1
fi
