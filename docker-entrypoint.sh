#!/bin/bash
mkdir dockerbuild
cd dockerbuild
pwd
python ../configure.py --enable-optimize --sdks cs2 --hl2sdk-root /app/source/sdks --hl2sdk-manifests /app/source/hl2sdk-manifests --mms_path=/app/metamod-source
ambuild