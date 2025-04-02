#!/bin/sh

export HTDIR=test
valgrind ./ht -a foo -a bar -a bruh \
	-s bruh -d 1 -d 2 \
	-H bar -d 1 -d 2 \
	-H foo -d 1 -d 2 \
	-H bruh -d 03.30 \
	-c -C -c bar -foo
