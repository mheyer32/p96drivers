#!/bin/bash
# Push only TestMach32 to the Amiga (same mechanism as squirt.sh).
# Build first: make TestMach32  (outputs _bin/TestMach32)

set -e
set -x

SQUIRT_HOST=${SQUIRT_HOST:=192.168.0.110}
SQUIRT_PATH=${SQUIRT_PATH:=~/squirt/build}
SQUIRT=${SQUIRT:=${SQUIRT_PATH}/squirt}

ROOT="$(cd "$(dirname "$0")" && pwd)"
"${SQUIRT}" --dest SYS:c "${SQUIRT_HOST}" "${ROOT}/_bin/TestMach32"
