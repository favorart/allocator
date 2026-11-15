#!/bin/bash
# next-tag.sh: the release tag of HEAD - its own tag if it has one, otherwise
# the next patch after the latest tag (v0.0.1 when there is none).
# Needs the full history (checkout with fetch-depth: 0).
set -e
own=$(git tag --points-at HEAD --list 'v[0-9]*' | sort -V | tail -n 1)
if [ -n "$own" ]; then
    echo "$own"
    exit 0
fi
last=$(git describe --tags --abbrev=0 --match 'v[0-9]*' 2>/dev/null || echo v0.0.0)
IFS=. read -r major minor patch <<< "${last#v}"
echo "v$major.$minor.$((patch + 1))"
