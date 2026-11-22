#!/bin/bash
# Task 5.3: evidence of the block-device interface, buffering and caching on this machine
echo "=== Device files: first letter b = block device, c = character device ==="
ls -l /dev/sda /dev/sdd /dev/null /dev/tty
echo
echo "=== Block device drivers registered in the kernel ==="
grep -A 12 '^Block devices' /proc/devices
echo
echo "=== Memory used as buffers, page cache and dirty (not yet written) data ==="
grep -E '^(MemTotal|Buffers|Cached|Dirty|Writeback):' /proc/meminfo
