#!/bin/bash

if ! type zcc 2>/dev/null; then
    echo "ERROR: no z88dk in PATH"
    exit 99
fi

set -e

DIST=dist
ZIP=t2esx.zip

# all clones with overclocking
make T2ESX_CPUFREQ=1 clean dist
mv "$DIST/$ZIP" "$DIST/t2esx-cpu.zip"

# ZX Spectrum Next
make T2ESX_NEXT=1 clean dist
mv "$DIST/$ZIP" "$DIST/t2esx-next.zip"

# original 48/128 with 2x loading
make T2ESX_TURBO=1 clean dist
mv "$DIST/$ZIP" "$DIST/t2esx-48k.zip"

# all features, cross-platform, CPU clock detection and 2x loading speed
# NOTE: 2x loading speed does NOT work with overlocked CPU
make T2ESX_TURBO=1 T2ESX_CPUFREQ=1 clean dist
mv "$DIST/$ZIP" "$DIST/t2esx-all.zip"

# EOF vim: ts=4:sw=4:et:ai: