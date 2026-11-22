#!/bin/bash
# Task 5 calculations. [A] = assumption, [C] = calculated.
echo "=== 5.2 Chance of losing all copies (assumption [A]: copies fail independently, p = chance a copy is lost before it is re-replicated) ==="
awk 'BEGIN{ p=0.01; printf "p = %.4f\n1 copy : %.6f\n2 copies: %.6f\n3 copies: %.6f (= p x p x p)\n", p, p, p*p, p*p*p }'

echo
echo "=== 5.2 Quorum rule: strong consistency needs R + W > N ==="
N=3
for W in 1 2 3; do
  for R in 1 2 3; do
    if [ $((R + W)) -gt $N ]; then S="strong"; else S="may read stale data"; fi
    echo "N=$N W=$W R=$R  R+W=$((R + W))  -> $S"
  done
done

echo
echo "=== Alternative: one NFS server against a replicated cluster ==="
echo "Assumptions [A]: file 100 MB; client link 1 Gbit/s = 125 MB/s; NFS server link 10 Gbit/s = 1250 MB/s;"
echo "cluster of 10 nodes, each with a 10 Gbit/s link; 3 replicas; disks are not the limit; all users busy at the same time."
awk 'BEGIN{
  file=100; client=125; nfs=1250; k=10; node=1250; rep=3
  readagg=k*node; writeagg=k*node/rep
  printf "%-7s %14s %14s %14s\n", "users", "NFS rd/wr (s)", "cluster rd (s)", "cluster wr (s)"
  n=split("1 10 100 1000", U, " ")
  for (i=1;i<=n;i++){
    u=U[i]
    r1=nfs/u;      if (r1>client) r1=client
    r2=readagg/u;  if (r2>client) r2=client
    r3=writeagg/u; if (r3>client) r3=client
    printf "%-7d %14.1f %14.1f %14.1f\n", u, file/r1, file/r2, file/r3
  }
  printf "\nNFS: per-user rate with 1000 users = 1250 / 1000 = %.2f MB/s\n", nfs/1000
  printf "Cluster read: 10 x 1250 = %d MB/s in total; write: %d / 3 = %.0f MB/s in total\n", readagg, readagg, writeagg
  printf "\nStorage for 100 TB of user data: NFS one copy = 100 TB; 3 replicas = 300 TB (2 TB of extra copies for every 1 TB)\n"
  a=0.99
  printf "\nAvailability (assumption [A]: each server is up 99%% of the time, failures independent)\n"
  printf "One NFS server : %.4f  (down %.1f hours per year)\n", a, (1-a)*8760
  printf "3 replicas     : %.6f  (all three down %.0f seconds per year)\n", 1-(1-a)^3, (1-a)^3*8760*3600
}'
