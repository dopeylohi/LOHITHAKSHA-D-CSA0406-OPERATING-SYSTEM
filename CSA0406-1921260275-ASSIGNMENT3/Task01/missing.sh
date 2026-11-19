#!/bin/bash
cd "$(dirname "$0")" || exit 1
cp libbankenc_v1.so libbankenc.so
trap 'mv -f libbankenc.so.hidden libbankenc.so 2>/dev/null' EXIT
mv libbankenc.so libbankenc.so.hidden
echo "--- library hidden. Dynamically linked app:"
./app_dyn; echo "exit code: $?"
echo "--- static app (does not need the .so):"
./app_static | head -1; echo "exit code: ${PIPESTATUS[0]}"
echo "--- dlopen app, encryption needed:"
./dl_app enc; echo "exit code: $?"
echo "--- dlopen app, encryption not needed:"
./dl_app none; echo "exit code: $?"
