#!/bin/bash

# Test reading from an execfs file.

if [ $# -ne 1 ]; then
    echo "Usage: $0 mountpoint" >&2
    exit 1
fi

OUTPUT=`cat "$1/file"`
if [ $? -ne 0 ]; then
    echo "Failed to read from file." >&2
    exit 1
elif [ "${OUTPUT}" != "hello world" ]; then
    echo "Incorrect output received." >&2
    exit 1
fi
