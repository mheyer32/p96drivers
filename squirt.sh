#!/bin/bash

set -x


SQUIRT_HOST=${SQUIRT_HOST:=192.168.0.110}
SQUIRT_PATH=${SQUIRT_PATH:=~/squirt/build}
SQUIRT=${SQUIRT:=${SQUIRT_PATH}/squirt}
SQUIRT_EXEC=${SQUIRT_EXEC:=${SQUIRT_PATH}/squirt_exec}

${SQUIRT} --dest boot:libs/picasso96 ${SQUIRT_HOST} $PWD/_bin/S3Trio64Plus.chip
${SQUIRT} --dest boot:libs/picasso96 ${SQUIRT_HOST} $PWD/_bin/S3Trio3264.chip
${SQUIRT} --dest boot:libs/picasso96 ${SQUIRT_HOST} $PWD/_bin/S3Vision864.chip

