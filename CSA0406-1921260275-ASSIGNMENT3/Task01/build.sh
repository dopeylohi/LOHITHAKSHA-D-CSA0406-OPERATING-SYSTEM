#!/bin/bash
set -e
cd "$(dirname "$0")"
W="-std=c11 -Wall -Wextra -O2"
set -x
gcc $W -fPIC -shared -o libbankenc.so bankenc.c
gcc $W -DV2 -fPIC -shared -o libbankenc_v2.so bankenc.c
cp libbankenc.so libbankenc_v1.so
gcc $W -static -o app_static app.c bankenc.c
gcc $W -o app_dyn app.c -L. -lbankenc -Wl,-rpath,'$ORIGIN'
gcc $W -o dl_app dl_app.c
