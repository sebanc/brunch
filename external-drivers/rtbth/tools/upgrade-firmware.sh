#! /usr/bin/env sh

# this is firmware upgrade tool for rtbt binary blob

dd if=rt3298_new.bin of=rtbt obs=1 seek=$((0xF4320)) conv=notrunc
