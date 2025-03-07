#!/bin/sh

COMPILER=gcc
DEBUG=

which tcc 2>/dev/null >/dev/null
if [ $? = "0" ]; then
	COMPILER=tcc
fi

if [ "$1" = "-r" ]; then
	DEBUG=
else
	COMPILER=gcc
	DEBUG=-g
fi

set -xe
$COMPILER $DEBUG main.c -o ht
