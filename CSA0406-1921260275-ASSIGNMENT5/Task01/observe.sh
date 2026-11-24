#!/usr/bin/env bash
# B1.1: one-shot snapshot of processes, memory, disk and network.
echo "### ps aux --sort=-%mem | head -8";  ps aux --sort=-%mem | head -8
echo; echo "### top -bn1 | head -15";       top -bn1 | head -15
echo; echo "### free -h";                   free -h
echo; echo "### vmstat 1 5";                vmstat 1 5
echo; echo "### iostat -x 1 3";             iostat -x 1 3
echo; echo "### df -h";                     df -h
echo; echo "### ss -tulpn";                 ss -tulpn
