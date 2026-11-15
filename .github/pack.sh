#!/bin/sh
# pack.sh <build dir>: the header and the static library built there
# -> allocator-<OS>-<arch>.tar.gz (a release asset, see ci.yml)
set -e
mkdir -p pkg/include pkg/lib
cp include/allocator.h pkg/include/
cp LICENSE README.md pkg/
find "$1" -maxdepth 2 -type f \( -name liballocator.a -o -name allocator.lib \) -exec cp {} pkg/lib/ \;
test -n "$(ls pkg/lib)"
tar -czf "allocator-$RUNNER_OS-$RUNNER_ARCH.tar.gz" -C pkg .
