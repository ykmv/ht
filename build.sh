#!/bin/sh

COMPILER=gcc
DEBUG=

which tcc 2>/dev/null >/dev/null
if [ $? = "0" ]; then
	COMPILER=tcc
fi

if [ "$1" = "-i" ]; then
	set -xe
	cp ht ~/.local/bin
	exit;
fi

if [ "$1" = "-r" ]; then
	DEBUG=
else 
	DEBUG=-gdwarf
fi

set -xe
$COMPILER $DEBUG main.c -o ht
