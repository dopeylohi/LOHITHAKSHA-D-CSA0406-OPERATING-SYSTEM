#!/bin/bash
# Task 6 calculations. [A] = assumption, [C] = calculated, [T] = typical published value.
awk 'BEGIN{
  students=200000; hours=5; peakf=3; vsize=1000; vmin=60; nvid=2000; hot=200
  hdd_pos=8.2; hdd_bw=150; nic=3125; nvme=3000; head=0.7

  print "=== Stage 1/4: workload (assumptions [A]: 5 viewing hours per student per week, peak = 3 x average, 1000 MB video of 60 min, 2000 videos) ==="
  conc = students*hours/168*peakf
  rate = vsize/(vmin*60)
  printf "Concurrent viewers at peak = %d x %d / 168 x %d = %.0f\n", students, hours, peakf, conc
  printf "Bitrate of one stream      = %d MB / %d s = %.4f MB/s = %.2f Mbit/s\n", vsize, vmin*60, rate, rate*8
  printf "Total read rate at peak    = %.0f x %.4f = %.0f MB/s = %.1f Gbit/s\n", conc, rate, conc*rate, conc*rate*8/1000
  printf "Traffic per month          = %d x %d x 4.33 weeks x %d MB/hour = %.0f GB = %.2f PB\n", students, hours, 3600*rate, students*hours*4.33*3600*rate/1000, students*hours*4.33*3600*rate/1e9

  print ""
  print "=== Stage 4: capacity ==="
  lib = nvid*vsize/1000
  printf "Library = %d videos x %d MB = %.0f GB\n", nvid, vsize, lib
  printf "On HDD with 2 replicas = %.0f GB; hot set on SSD = %d videos x %d MB = %.0f GB; cloud copy = %.0f GB\n", lib*2, hot, vsize, hot*vsize/1000, lib
  printf "With 3 replicas instead of 2: %.0f GB on HDD\n", lib*3
  p=0.01
  printf "Chance all copies are lost (p = %.2f per copy [A], independent): 2 copies %.6f, 3 copies %.6f\n", p, p*p, p*p*p

  print ""
  print "=== Stage 3: request size on a hard disk (positioning 8.2 ms [A], 150 MB/s [A]) ==="
  printf "%-10s %12s %12s %14s %16s\n", "size", "transfer ms", "efficiency", "reads/s/HDD", "streams/HDD"
  n=split("4 64 256 1024 4096 16384", S, " ")
  for (i=1;i<=n;i++){
    mb=S[i]*1024/1000000; t=mb/hdd_bw*1000; eff=t/(hdd_pos+t); rps=1000/(hdd_pos+t)
    printf "%-7s KiB %12.2f %11.0f%% %14.1f %16.0f\n", S[i], t, eff*100, rps, rps*mb/rate
  }
  mb=4194304/1000000; t=mb/hdd_bw*1000; rps=1000/(hdd_pos+t); spd=rps*mb/rate
  printf "Chosen: 4 MiB. One chunk = %.1f s of video, so each stream needs one read every %.1f s\n", mb/rate, mb/rate

  print ""
  print "=== Stage 4: servers needed (per server: 12 HDD, 25 Gbit/s link, one 3000 MB/s NVMe in design B; 70% headroom) ==="
  hddcap = 12*spd; nicstreams = nic/rate; nvmestreams = nvme/rate
  sumall=0; sumhot=0
  for (v=1; v<=nvid; v++){ sumall+=1/v; if (v<=hot) sumhot+=1/v }
  hit = sumhot/sumall
  printf "Hit ratio of the %d most popular videos with Zipf popularity (1/rank) = %.3f\n", hot, hit
  printf "Streams per server: HDD side %.0f, network %.0f, NVMe %.0f\n", hddcap, nicstreams, nvmestreams
  sa = hddcap; if (nicstreams < sa) sa = nicstreams
  sb = hddcap/(1-hit); x = nvmestreams/hit; if (x < sb) sb = x; if (nicstreams < sb) sb = nicstreams
  printf "Design A (HDD only):     limit %.0f streams, with headroom %.0f, servers = ceil(%.0f / %.0f) = %d\n", sa, sa*head, conc, sa*head, int(conc/(sa*head))+(conc/(sa*head)>int(conc/(sa*head)))
  printf "Design B (SSD + HDD):    limit %.0f streams, with headroom %.0f, servers = ceil(%.0f / %.0f) = %d\n", sb, sb*head, conc, sb*head, int(conc/(sb*head))+(conc/(sb*head)>int(conc/(sb*head)))

  print ""
  print "=== Stage 4: disk reads to start playing at minute 59 of a 60-minute, 1000 MB video (4 KiB blocks) ==="
  blocks = vsize*1000000/4096; blk = int(blocks*59/60); p=1024
  printf "Blocks in file = %.0f; block at minute 59 = %d; double-indirect range starts at %d\n", blocks, blk, 12+p
  printf "Classic inode (Task 2): inode 1 + container index 1 + double-indirect 1 + single-indirect 1 + data chunk 1 = 5 reads\n"
  printf "Extent-based (XFS/ext4): inode 1 + container index 1 + data chunk 1 = 3 reads (4 if the extent list is too long for the inode)\n"
  tx=4194304/1000000/hdd_bw*1000; ts=4194304/1000000/nvme*1000
  printf "Time on HDD  : 5 reads = 4 x 8.2 + (8.2 + %.1f) = %.1f ms ; 3 reads = 2 x 8.2 + (8.2 + %.1f) = %.1f ms\n", tx, 4*8.2+8.2+tx, tx, 2*8.2+8.2+tx
  printf "Time on SSD  : 5 reads = 4 x 0.1 + (0.1 + %.1f) = %.1f ms ; 3 reads = %.1f ms\n", ts, 4*0.1+0.1+ts, 2*0.1+0.1+ts
  printf "If inode and container index are already cached in RAM: 1 read = %.1f ms on HDD, %.1f ms on SSD\n", hdd_pos+tx, 0.1+ts

  print ""
  seg = 2*rate; dl = seg/2.5*1000
  printf "=== Alternative: cloud object storage + CDN (assumptions [A]: origin in Chennai, home link 20 Mbit/s, first 2 s of video = %.3f MB) ===\n", seg
  edge=15; edgehit=0.95
  printf "Download of the first segment = %.3f MB / 2.5 MB/s = %.0f ms\n", seg, dl
  printf "%-10s %12s %22s %22s\n", "city", "RTT origin", "start, own server ms", "start, CDN edge ms"
  split("Chennai Bengaluru Mumbai Kolkata Delhi", C, " ")
  split("10 25 45 60 70", R, " ")
  for (i=1;i<=5;i++){
    own = 3*R[i] + dl + 52.6
    cdn = 3*edge + dl + 5 + (1-edgehit)*(20+52.6)
    printf "%-10s %9d ms %22.1f %22.1f\n", C[i], R[i], own, cdn
  }
  print "Start time = 3 round trips (connection, TLS, request) + first segment + read time; own server read = 52.6 ms (3 reads on HDD, from above); CDN edge read 5 ms, 5% misses add 20 ms + 52.6 ms"
  tb = students*hours*4.33*3600*rate/1000
  print ""
  print "=== Alternative: monthly cost (prices [A], blended after volume discounts; real quotes differ) ==="
  printf "Object storage: %.0f GB x $0.023 = $%.0f per month [T price]\n", lib, lib*0.023
  printf "CDN transfer %.0f GB: at $0.02/GB = $%.0f ; $0.03/GB = $%.0f ; $0.05/GB = $%.0f per month\n", tb, tb*0.02, tb*0.03, tb*0.05
  printf "Own servers, internet transit %.0f GB: at $0.005/GB = $%.0f ; $0.01/GB = $%.0f per month (plus the servers and staff)\n", tb, tb*0.005, tb*0.01
}'
