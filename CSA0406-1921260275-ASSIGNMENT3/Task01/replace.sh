#!/bin/bash
cd "$(dirname "$0")" || exit 1
cp libbankenc_v1.so libbankenc.so
echo "--- BEFORE: original library v1"
md5sum app_dyn libbankenc.so
./app_dyn | head -3
echo "--- replace ONLY the shared library with v2 (no recompile, no relink)"
cp libbankenc_v2.so libbankenc.so
md5sum app_dyn libbankenc.so
./app_dyn | head -3
echo "--- static build is unchanged (needs a rebuild to get the fix)"
./app_static | head -1
cp libbankenc_v1.so libbankenc.so
