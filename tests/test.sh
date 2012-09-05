#!/bin/bash

# Mount an execfs file system, run a test on it, unmount it and delete the
# mount point.

if [ $# -ne 2 ]; then
    echo "Usage: $0 script config" >&2
    exit 1
fi

MOUNT=`mktemp -d`
execfs --config "$2" --fuse "${MOUNT}" && \
 "$1" "${MOUNT}" && \
 fusermount -uz "${MOUNT}" && \
 rm -rf "${MOUNT}"
