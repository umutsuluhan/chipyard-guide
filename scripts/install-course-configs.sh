#!/bin/bash
set -e

WS="${WS:-/workspace}"

SRC="$WS/course/configs"
DST="$WS/chipyard/generators/chipyard/src/main/scala/config"

if [ ! -d "$SRC" ]; then
    echo "Course configuration directory not found: $SRC"
    exit 1
fi

echo "Linking course Chipyard configurations..."

for file in "$SRC"/*.scala; do
    if [ -e "$file" ]; then
        name=$(basename "$file")

        ln -sf "$file" "$DST/$name"

        echo "  $name"
    fi
done

echo "Course configurations ready."