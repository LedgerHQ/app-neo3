#!/usr/bin/env bash
set -e

SCRIPTDIR="$(cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd)"
BUILDDIR="$SCRIPTDIR/cmake-build-fuzz"
CORPUSDIR="$SCRIPTDIR/corpus"

[[ ! -d "$CORPUSDIR" ]] && mkdir "$CORPUSDIR"

"$BUILDDIR"/fuzz_message "$CORPUSDIR" "$@" > /dev/null
