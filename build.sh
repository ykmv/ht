#!/bin/sh

DEBUG=
if [ "$1" = "-r" ]; then
	DEBUG=
else
	DEBUG=-g
fi
gcc $DEBUG main.c -o ht
