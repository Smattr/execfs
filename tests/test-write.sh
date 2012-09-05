#!/bin/bash

# Test writing from an execfs file.

if [ $# -ne 1 ]; then
    echo $#
    echo "Usage: $0 mountpoint" >&2
    exit 1
fi

echo "hello world" >"$1/file"
if [ $? -ne 0 ]; then
    echo "Failed to write to file." >&2
    exit 1
fi
OUTPUT="`cat /tmp/_execfs_test-write.config.testing`"
if [ $? -ne 0 ]; then
    echo "Failed to read back written contents." >&2
    exit 1
elif [ "${OUTPUT}" != "hello world" ]; then
    echo "Incorrect output received." >&2
    exit 1
fi
