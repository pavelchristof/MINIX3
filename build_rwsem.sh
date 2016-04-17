#! /usr/bin/env sh
make includes
cd servers/ipc/
make && make install
cd ../../lib/libc/
make && make install
reboot
